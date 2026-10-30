#pragma once

#include <esp_now.h>

#include "feature.h"

// Device runtime: owns ESP-NOW setup, the boot announcement and dispatching
// of core requests to registered features. Device firmwares only declare
// their name and features.
class MandosDevice {
public:
    explicit MandosDevice(const char *name) : name_(name) {}

    // Call before begin(). Returns false if the feature table is full.
    bool addFeature(Feature &feature);

    bool begin();

    // `commandId` is the id of the Read being answered, 0 for spontaneous notifications.
    bool sendValue(uint8_t index, const uint8_t *data, size_t len, uint16_t commandId = 0);

private:
    static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
    void handle(const uint8_t *data, int len);
    void announce();

    const char *name_;
    Feature *features_[MANDOS_MAX_FEATURES] = {};
    uint8_t featureCount_ = 0;
    bool started_ = false;

    static MandosDevice *instance_;
};
