#pragma once

#include <stddef.h>
#include <stdint.h>

#include <device_registry.h>

// ESP-NOW requests from the core to devices.
namespace core_radio {

// Registers the send callback: a failed delivery marks the device offline in `registry`.
// Call after the radio is initialised.
void begin(DeviceRegistry &registry);

bool sendWrite(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len);
bool sendRead(const uint8_t mac[6], uint8_t featureIndex);
// Asks the device to announce itself again.
bool sendPing(const uint8_t mac[6]);

}
