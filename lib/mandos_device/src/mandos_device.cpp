#include "mandos_device.h"

#include <Arduino.h>
#include <radio.h>

namespace {
const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
}

MandosDevice *MandosDevice::instance_ = nullptr;

bool Feature::notify(const uint8_t *data, size_t len) {
    return device_ && device_->sendValue(index_, data, len);
}

bool MandosDevice::addFeature(Feature &feature) {
    if (featureCount_ >= MANDOS_MAX_FEATURES || started_) {
        return false;
    }
    feature.device_ = this;
    feature.index_ = featureCount_;
    features_[featureCount_++] = &feature;
    return true;
}

bool MandosDevice::begin() {
    if (mandos::radioBegin() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }

    instance_ = this;
    esp_now_register_recv_cb(onRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcast, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("ESP-NOW add peer failed");
        return false;
    }

    started_ = true;
    announce();
    return true;
}

void MandosDevice::announce() {
    AnnounceMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.header = makeHeader(MsgType::Announce);
    strncpy(msg.name, name_, sizeof(msg.name) - 1);
    msg.feature_count = featureCount_;
    for (uint8_t i = 0; i < featureCount_; i++) {
        strncpy(msg.features[i].name, features_[i]->name(), sizeof(msg.features[i].name) - 1);
        msg.features[i].type = static_cast<uint8_t>(features_[i]->type());
        msg.features[i].flags = features_[i]->flags();
    }

    esp_now_send(kBroadcast, (const uint8_t *)&msg, sizeof(msg));
}

// Notifications are broadcast: the device does not need to know the core's MAC.
bool MandosDevice::sendValue(uint8_t index, const uint8_t *data, size_t len, uint16_t commandId) {
    if (!started_ || len > MANDOS_MAX_VALUE) {
        return false;
    }

    ValueMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.header = makeHeader(MsgType::Notify);
    msg.command_id = commandId;
    msg.feature_index = index;
    msg.length = len;
    memcpy(msg.data, data, len);

    return esp_now_send(kBroadcast, (const uint8_t *)&msg, valueMessageSize(len)) == ESP_OK;
}

void MandosDevice::onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (instance_) {
        instance_->handle(data, len);
    }
}

void MandosDevice::handle(const uint8_t *data, int len) {
    MsgType type;
    if (!parseHeader(data, len, type)) {
        return;
    }

    if (type == MsgType::Ping && len == (int)sizeof(PingMessage)) {
        announce();
    } else if (type == MsgType::Read && len == (int)sizeof(ReadMessage)) {
        ReadMessage msg;
        memcpy(&msg, data, sizeof(msg));
        if (msg.feature_index >= featureCount_ || !(features_[msg.feature_index]->flags() & FEATURE_READ)) {
            return;
        }
        uint8_t buf[MANDOS_MAX_VALUE];
        size_t n = features_[msg.feature_index]->onRead(buf, sizeof(buf));
        sendValue(msg.feature_index, buf, n, msg.command_id);
    } else if (type == MsgType::Write && len >= (int)valueMessageSize(0)) {
        ValueMessage msg;
        memset(&msg, 0, sizeof(msg));
        memcpy(&msg, data, len < (int)sizeof(msg) ? len : sizeof(msg));
        if (msg.length > MANDOS_MAX_VALUE || len != (int)valueMessageSize(msg.length)) {
            return;
        }
        if (msg.feature_index >= featureCount_ || !(features_[msg.feature_index]->flags() & FEATURE_WRITE)) {
            return;
        }
        features_[msg.feature_index]->onWrite(msg.data, msg.length);
    }
}
