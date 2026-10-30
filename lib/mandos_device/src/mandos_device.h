#pragma once

#include <esp_now.h>

#include "feature.h"

// Device runtime: owns ESP-NOW setup, periodic announcements and dispatching
// of core requests to registered features. Device firmwares only declare
// their name and features.
class MandosDevice {
public:
    explicit MandosDevice(const char *name) : name_(name) {}

    // Call before begin(). Returns false if the feature table is full.
    bool addFeature(Feature &feature);

    bool begin();
    // Call from loop(): sends the periodic announcement.
    void loop();

    bool sendValue(uint8_t index, const uint8_t *data, size_t len);

    void setAnnounceIntervalMs(unsigned long ms) { announceIntervalMs_ = ms; }

private:
    static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
    void handle(const uint8_t *data, int len);
    void announce();

    const char *name_;
    Feature *features_[MANDOS_MAX_FEATURES] = {};
    uint8_t featureCount_ = 0;
    unsigned long announceIntervalMs_ = 1000;
    unsigned long lastAnnounce_ = 0;
    bool started_ = false;

    static MandosDevice *instance_;
};
