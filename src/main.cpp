
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define LED_PIN 8

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *server;

class LEDCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String value = characteristic->getValue().c_str();

    if (value == "1") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    } else if (value == "0") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize BLE
  BLEDevice::init("CosplayOS-ESP32");

  server = BLEDevice::createServer();

  // Automatically advertise again after disconnection
  server->advertiseOnDisconnect(true);

  // Create service
  BLEService *service = server->createService(SERVICE_UUID);

  // Create writable LED characteristic
  BLECharacteristic *characteristic =
    service->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );

  characteristic->setCallbacks(new LEDCallbacks());

  service->start();

  // Start BLE advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();

  Serial.println("BLE ready");
  Serial.println("Advertising started");
}

void loop() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck >= 5000) {
    lastCheck = millis();

    Serial.printf(
      "Connected clients: %lu\n",
      (unsigned long)server->getConnectedCount()
    );
  }

  delay(100);
}
