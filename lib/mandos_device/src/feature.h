#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <protocol.h>

class MandosDevice;

// A single capability of a device (button, LED, text display, sensor...).
// Subclass it, override the handlers it supports, and register it with
// MandosDevice::addFeature(). Handlers run on the WiFi task: keep them short.
class Feature {
public:
    Feature(const char *name, FeatureType type, uint8_t flags)
        : name_(name), type_(type), flags_(flags) {}
    virtual ~Feature() = default;

    // Core wrote a value. Return false if it is invalid.
    virtual bool onWrite(const uint8_t *data, size_t len) { return false; }

    // Fill `out` (capacity `max`) with the current value, return its length.
    virtual size_t onRead(uint8_t *out, size_t max) { return 0; }

    const char *name() const { return name_; }
    FeatureType type() const { return type_; }
    uint8_t flags() const { return flags_; }

protected:
    // Pushes a value to the core. No-op until the feature is registered.
    bool notify(const uint8_t *data, size_t len);
    bool notifyBool(bool v) { uint8_t b = v; return notify(&b, 1); }
    bool notifyString(const char *s) { return notify((const uint8_t *)s, strlen(s)); }

private:
    friend class MandosDevice;
    const char *name_;
    FeatureType type_;
    uint8_t flags_;
    MandosDevice *device_ = nullptr;
    uint8_t index_ = 0;
};
