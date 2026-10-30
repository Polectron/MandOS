#include "ble_service.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <deque>
#include <mutex>
#include <string>

#include "core_radio.h"

namespace core_ble {

namespace {

constexpr size_t kAddrLen = 7;  // mac + feature index
constexpr size_t kMaxQueued = 16;

DeviceRegistry *g_registry = nullptr;
NimBLECharacteristic *g_value = nullptr;

// Filled from the ESP-NOW task, drained from loop().
std::mutex g_queueMutex;
std::deque<std::string> g_queue;

// Rebuilds the list on every read so the client always sees the current registry.
class DevicesCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
        characteristic->setValue(g_registry->toJson());
    }
};

class WriteCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
        std::string data = characteristic->getValue();
        if (data.size() < kAddrLen) {
            return;
        }
        const uint8_t *mac = (const uint8_t *)data.data();
        uint8_t index = mac[6];
        const uint8_t *value = mac + kAddrLen;
        size_t len = data.size() - kAddrLen;

        if (!g_registry->checkAccess(mac, index, FEATURE_WRITE, len)) {
            return;
        }
        core_radio::sendWrite(mac, index, value, len);
    }
};

class ValueCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
        std::string data = characteristic->getValue();
        if (data.size() != kAddrLen) {
            return;
        }
        const uint8_t *mac = (const uint8_t *)data.data();
        if (!g_registry->checkAccess(mac, mac[6], FEATURE_READ)) {
            return;
        }
        core_radio::sendRead(mac, mac[6]);
    }
};

}

void begin(const char *name, DeviceRegistry &registry) {
    g_registry = &registry;

    NimBLEDevice::init(name);
    NimBLEDevice::setMTU(247);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->advertiseOnDisconnect(true);

    NimBLEService *service = server->createService(MANDOS_BLE_SERVICE_UUID);

    NimBLECharacteristic *devices = service->createCharacteristic(
        MANDOS_BLE_DEVICES_UUID, NIMBLE_PROPERTY::READ);
    devices->setCallbacks(new DevicesCallbacks());
    devices->setValue(registry.toJson());

    NimBLECharacteristic *write = service->createCharacteristic(
        MANDOS_BLE_WRITE_UUID, NIMBLE_PROPERTY::WRITE);
    write->setCallbacks(new WriteCallbacks());

    g_value = service->createCharacteristic(
        MANDOS_BLE_VALUE_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    g_value->setCallbacks(new ValueCallbacks());

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(MANDOS_BLE_SERVICE_UUID);
    advertising->start();

    Serial.println("BLE advertising");
}

void publishValue(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len) {
    std::string payload((const char *)mac, 6);
    payload += (char)featureIndex;
    payload.append((const char *)value, len);

    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (g_queue.size() >= kMaxQueued) {
        g_queue.pop_front();
    }
    g_queue.push_back(std::move(payload));
}

void loop() {
    if (!g_value) {
        return;
    }
    for (;;) {
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (g_queue.empty()) {
                return;
            }
            payload = std::move(g_queue.front());
            g_queue.pop_front();
        }
        g_value->setValue(payload);
        g_value->notify();
    }
}

}
