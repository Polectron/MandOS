#pragma once

#include <stddef.h>
#include <stdint.h>

#include <device_registry.h>

// BLE GATT interface of the core, for phones / apps.
//
// Service MANDOS_BLE_SERVICE_UUID:
//
//   devices (READ)
//       JSON list of registered devices, see DeviceRegistry::toJson().
//       Long reads are supported, so the list may exceed the MTU.
//
//   write (WRITE)
//       Write a feature value on a device.
//       Payload: [mac:6][feature_index:1][value...]   (value encoded as in protocol.h)
//
//   value (WRITE | READ | NOTIFY)
//       Write [mac:6][feature_index:1] to request a read of that feature.
//       Every value coming from a device (read responses and spontaneous
//       notifications) is delivered here as [mac:6][feature_index:1][value...],
//       both as a notification and as the readable value.
//
// Requests for unknown devices/features, missing permissions or wrongly sized
// values are ignored.
namespace core_ble {

#define MANDOS_BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MANDOS_BLE_DEVICES_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MANDOS_BLE_WRITE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define MANDOS_BLE_VALUE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"

void begin(const char *name, DeviceRegistry &registry);

// Called from the ESP-NOW receive callback: queues a device value for BLE clients.
void publishValue(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len);

// Call from loop(): delivers queued values to BLE clients.
void loop();

}
