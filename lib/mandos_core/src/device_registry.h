#pragma once

#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "device_model.h"
#include "protocol.h"

// Human readable rendering of a raw feature value.
std::string formatValue(FeatureType type, const uint8_t *data, size_t len);

struct StoredDevice {
    std::array<uint8_t, 6> mac;
    AnnounceMessage announce;
};

// Devices seen by the core, keyed by MAC address ("AA:BB:CC:DD:EE:FF").
// Thread-safe: ESP-NOW callbacks run on the WiFi task, the main loop on another.
// Has no Arduino dependency.
class DeviceRegistry {
public:
    static std::string macToString(const uint8_t mac[6]);

    // Inserts or replaces the device announced by `msg` and marks it online.
    // Returns false if malformed.
    bool update(const uint8_t mac[6], const AnnounceMessage &msg, unsigned long nowMs);

    // Loads a persisted device as offline. Does not mark the registry dirty.
    bool restore(const uint8_t mac[6], const AnnounceMessage &msg);

    // Marks a known device online (any message from it proves it is alive).
    void markSeen(const uint8_t mac[6], unsigned long nowMs);

    // Records the ESP-NOW delivery result of a command sent to the device.
    // A failed delivery marks it offline, a successful one online.
    void setReachable(const uint8_t mac[6], bool reachable, unsigned long nowMs);

    // MACs of known devices that are currently offline.
    std::vector<std::array<uint8_t, 6>> offlineMacs();

    // True once if the persistent content (devices, names, features) changed since the last call.
    bool takeDirty();

    // Persistent content of every device, as announce messages.
    std::vector<StoredDevice> exportAll();

    // Describes a value message from a known device, e.g. "AA:..  keypad/combo = ...".
    // Returns an empty string if the device or feature is unknown.
    std::string describeValue(const uint8_t mac[6], const ValueMessage &msg);

    // True if the device is known, has `featureIndex`, the feature has all of `requiredFlags`
    // and, when `valueLen` >= 0, a value of that length is valid for its type.
    bool checkAccess(const uint8_t mac[6], uint8_t featureIndex, uint8_t requiredFlags, int valueLen = -1);

    std::vector<Device> snapshot();
    size_t size();
    std::string dump();

    // Compact JSON: [{"id":"MAC","name":"..","online":bool,"features":[{"name":"..","type":"..","flags":"RWN"}]}]
    std::string toJson();

private:
    bool upsert(const uint8_t mac[6], const AnnounceMessage &msg, unsigned long nowMs, bool online);

    std::mutex mutex_;
    bool dirty_ = false;
    std::map<std::string, Device> devices_;
};
