#pragma once

#include <esp_err.h>

namespace mandos {

// Brings up only the WiFi driver needed by ESP-NOW (no netif / lwIP / DHCP),
// then initialises ESP-NOW. Safe to call once at startup.
esp_err_t radioBegin();

}
