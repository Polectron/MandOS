#pragma once

#include <stddef.h>
#include <stdint.h>

// ESP-NOW requests from the core to devices.
namespace core_radio {

bool sendWrite(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len);
bool sendRead(const uint8_t mac[6], uint8_t featureIndex);

}
