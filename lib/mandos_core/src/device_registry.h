#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "device_model.h"
#include "protocol.h"

// Human readable rendering of a raw feature value.
std::string formatValue(FeatureType type, const uint8_t *data, size_t len);

// Devices seen by the core, keyed by MAC address ("AA:BB:CC:DD:EE:FF").
// Thread-safe: ESP-NOW callbacks run on the WiFi task, the main loop on another.
// Has no Arduino dependency.
class DeviceRegistry {
public:
    static std::string macToString(const uint8_t mac[6]);

    // Inserts or replaces the device announced by `msg`. Returns false if malformed.
    bool update(const uint8_t mac[6], const AnnounceMessage &msg, unsigned long nowMs);

    // Describes a value message from a known device, e.g. "AA:..  keypad/combo = ...".
    // Returns an empty string if the device or feature is unknown.
    std::string describeValue(const uint8_t mac[6], const ValueMessage &msg);

    // True if the device is known, has `featureIndex`, the feature has all of `requiredFlags`
    // and, when `valueLen` >= 0, a value of that length is valid for its type.
    bool checkAccess(const uint8_t mac[6], uint8_t featureIndex, uint8_t requiredFlags, int valueLen = -1);

    std::vector<Device> snapshot();
    size_t size();
    std::string dump();

    // Compact JSON: [{"id":"MAC","name":"..","features":[{"name":"..","type":"..","flags":"RWN"}]}]
    std::string toJson();

private:
    std::mutex mutex_;
    std::map<std::string, Device> devices_;
};
