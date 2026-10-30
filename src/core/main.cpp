#include <Arduino.h>
#include <radio.h>
#include <esp_now.h>
#include <device_registry.h>
#include <protocol.h>
#include "ble_service.h"
#include "core_radio.h"
#include "registry_store.h"

DeviceRegistry registry;

// Restored devices are pinged after boot until they answer or the window ends.
constexpr unsigned long kPingWindowMs = 6000;
constexpr unsigned long kPingIntervalMs = 1000;
unsigned long bootMs = 0;


void onDataRecv(
    const esp_now_recv_info_t *info,
    const uint8_t *data,
    int len
) {
    MsgType type;
    if (!parseHeader(data, len, type)) {
        return;
    }

    if (type == MsgType::Announce && len == (int)sizeof(AnnounceMessage)) {
        AnnounceMessage msg;
        memcpy(&msg, data, sizeof(msg));
        if (!registry.update(info->src_addr, msg, millis())) {
            Serial.println("Invalid announce message");
        }
    } else if (type == MsgType::Notify && len >= (int)valueMessageSize(0)) {
        ValueMessage msg;
        memset(&msg, 0, sizeof(msg));
        memcpy(&msg, data, len < (int)sizeof(msg) ? len : sizeof(msg));
        if (msg.length > MANDOS_MAX_VALUE || len != (int)valueMessageSize(msg.length)) {
            return;
        }
        registry.markSeen(info->src_addr, millis());
        std::string text = registry.describeValue(info->src_addr, msg);
        if (!text.empty()) {
            Serial.printf("Notify: %s\n", text.c_str());
            core_ble::publishValue(info->src_addr, msg.feature_index, msg.data, msg.length);
        }
    }
}

void setup() {
    Serial.begin(115200);

    if (mandos::radioBegin() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);

    core_radio::begin(registry);
    core_store::load(registry);
    bootMs = millis();

    Serial.println("Listening for broadcasts...");

    core_ble::begin("MandOS-Core", registry);
}

void loop() {
    static unsigned long lastDump = 0;
    if (millis() - lastDump >= 1000) {
        lastDump = millis();
        Serial.print(registry.dump().c_str());
    }

    static unsigned long lastPing = 0;
    if (millis() - bootMs < kPingWindowMs && millis() - lastPing >= kPingIntervalMs) {
        lastPing = millis();
        for (const auto &mac : registry.offlineMacs()) {
            core_radio::sendPing(mac.data());
        }
    }

    if (registry.takeDirty()) {
        core_store::save(registry);
    }

    core_ble::loop();
    delay(10);
}
