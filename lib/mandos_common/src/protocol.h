#pragma once

#include <stddef.h>
#include <stdint.h>

// Wire format shared by every MandOS firmware (core and all devices).
// ESP-NOW payloads are limited to 250 bytes. All messages start with a Header.

constexpr uint8_t MANDOS_PROTOCOL_VERSION = 2;
constexpr size_t MANDOS_NAME_LEN = 16;
constexpr size_t MANDOS_MAX_FEATURES = 12;
constexpr size_t MANDOS_MAX_VALUE = 64;

enum class MsgType : uint8_t {
    Announce = 1,  // device -> core (broadcast): who I am and my features
    Read = 2,      // core -> device: request the current value of a feature
    Write = 3,     // core -> device: set a feature value
    Notify = 4,    // device -> core: a feature value changed / read response
};

// Value encodings (little endian):
//   Bool     1 byte, 0 or 1
//   Int32    4 bytes
//   Float    4 bytes (IEEE 754)
//   String   raw bytes, not null-terminated
//   KeyCombo byte 0 = modifier bitmask (see KEYMOD_*), following bytes = HID key codes
enum class FeatureType : uint8_t {
    Bool = 1,
    Int32 = 2,
    Float = 3,
    String = 4,
    KeyCombo = 5,
};

constexpr uint8_t FEATURE_READ = 0x01;
constexpr uint8_t FEATURE_WRITE = 0x02;
constexpr uint8_t FEATURE_NOTIFY = 0x04;

constexpr uint8_t KEYMOD_CTRL = 0x01;
constexpr uint8_t KEYMOD_SHIFT = 0x02;
constexpr uint8_t KEYMOD_ALT = 0x04;
constexpr uint8_t KEYMOD_GUI = 0x08;

struct __attribute__((packed)) Header {
    uint8_t version;
    uint8_t type;  // MsgType
};

struct __attribute__((packed)) FeatureInfo {
    char name[MANDOS_NAME_LEN];
    uint8_t type;  // FeatureType
    uint8_t flags;
};

struct __attribute__((packed)) AnnounceMessage {
    Header header;
    char name[MANDOS_NAME_LEN];
    uint8_t feature_count;
    FeatureInfo features[MANDOS_MAX_FEATURES];
};

// Features are addressed by their index in the announced list.
struct __attribute__((packed)) ReadMessage {
    Header header;
    uint8_t feature_index;
};

// Used for both Write and Notify.
struct __attribute__((packed)) ValueMessage {
    Header header;
    uint8_t feature_index;
    uint8_t length;
    uint8_t data[MANDOS_MAX_VALUE];
};

static_assert(sizeof(AnnounceMessage) <= 250, "Announce exceeds ESP-NOW payload");
static_assert(sizeof(ValueMessage) <= 250, "Value message exceeds ESP-NOW payload");

// Size on the wire of a ValueMessage carrying `length` bytes.
constexpr size_t valueMessageSize(size_t length) {
    return offsetof(ValueMessage, data) + length;
}

inline Header makeHeader(MsgType type) {
    return Header{MANDOS_PROTOCOL_VERSION, static_cast<uint8_t>(type)};
}

// Returns true when `data` holds a header with a supported protocol version.
inline bool parseHeader(const uint8_t *data, int len, MsgType &type) {
    if (len < (int)sizeof(Header) || data[0] != MANDOS_PROTOCOL_VERSION) {
        return false;
    }
    type = static_cast<MsgType>(data[1]);
    return true;
}

inline const char *featureTypeName(FeatureType type) {
    switch (type) {
        case FeatureType::Bool: return "bool";
        case FeatureType::Int32: return "int32";
        case FeatureType::Float: return "float";
        case FeatureType::String: return "string";
        case FeatureType::KeyCombo: return "keycombo";
    }
    return "unknown";
}
