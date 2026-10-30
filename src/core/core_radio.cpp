#include "core_radio.h"

#include <esp_now.h>
#include <string.h>

#include <protocol.h>

namespace core_radio {

namespace {

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

bool sendWrite(const uint8_t mac[6], uint8_t featureIndex, const uint8_t *value, size_t len) {
    if (len > MANDOS_MAX_VALUE || !ensurePeer(mac)) {
        return false;
    }

    ValueMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.header = makeHeader(MsgType::Write);
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
    msg.feature_index = featureIndex;
    return esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg)) == ESP_OK;
}

}
