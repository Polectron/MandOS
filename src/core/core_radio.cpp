#include "core_radio.h"

#include <Arduino.h>
#include <esp_now.h>
#include <string.h>

#include <protocol.h>

namespace core_radio {

namespace {

DeviceRegistry *g_registry = nullptr;

uint16_t nextCommandId() {
    static uint16_t counter = 0;
    if (++counter == 0) counter = 1;
    return counter;
}

// Runs on the WiFi task. All core commands are unicast, so the result reflects
// whether the device's radio acknowledged the frame.
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
    if (g_registry && info && info->des_addr) {
        g_registry->setReachable(info->des_addr, status == ESP_NOW_SEND_SUCCESS, millis());
    }
}

bool ensurePeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

}

void begin(DeviceRegistry &registry) {
    g_registry = &registry;
    esp_now_register_send_cb(onSent);
}

bool sendWrite(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len) {
    if (len > MANDOS_MAX_VALUE || !ensurePeer(mac)) {
        return false;
    }

    ValueMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.header = makeHeader(MsgType::Write);
    msg.command_id = nextCommandId();
    msg.feature_index = featureIndex;
    msg.length = len;
    memcpy(msg.data, value, len);
    return esp_now_send(mac, (const uint8_t *)&msg, valueMessageSize(len)) == ESP_OK;
}

bool sendRead(const uint8_t mac[6], uint8_t featureIndex) {
    if (!ensurePeer(mac)) {
        return false;
    }

    ReadMessage msg;
    msg.header = makeHeader(MsgType::Read);
    msg.command_id = nextCommandId();
    msg.feature_index = featureIndex;
    return esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg)) == ESP_OK;
}

bool sendPing(const uint8_t mac[6]) {
    if (!ensurePeer(mac)) {
        return false;
    }

    PingMessage msg;
    msg.header = makeHeader(MsgType::Ping);
    msg.command_id = nextCommandId();
    return esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg)) == ESP_OK;
}

}
