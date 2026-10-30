#pragma once

#include <array>
#include <string>
#include <vector>

#include "protocol.h"

class DeviceCapability {
    public:
    std::string name;
    FeatureType type = FeatureType::Bool;
    bool can_read = false;
    bool can_write = false;
    bool can_notify = false;
};

class Device {
    public:
    std::string name;
    std::string mac;
    std::array<uint8_t, 6> mac_raw{};
    unsigned long last_seen_ms = 0;
    // Position in the vector is the feature index used on the wire.
    std::vector<DeviceCapability> capabilities;
};
