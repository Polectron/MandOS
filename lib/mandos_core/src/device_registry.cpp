#include "device_registry.h"

#include <stdio.h>
#include <string.h>

namespace {
// Wire strings may not be null-terminated.
std::string boundedString(const char *s, size_t max) {
    return std::string(s, strnlen(s, max));
}

bool validFeatureType(uint8_t t) {
    return t >= (uint8_t)FeatureType::Bool && t <= (uint8_t)FeatureType::KeyCombo;
}

bool sameContent(const Device &a, const Device &b) {
    if (a.name != b.name || a.capabilities.size() != b.capabilities.size()) {
        return false;
    }
    for (size_t i = 0; i < a.capabilities.size(); i++) {
        const DeviceCapability &x = a.capabilities[i], &y = b.capabilities[i];
        if (x.name != y.name || x.type != y.type || x.can_read != y.can_read ||
            x.can_write != y.can_write || x.can_notify != y.can_notify) {
            return false;
        }
    }
    return true;
}

std::string jsonEscape(const std::string &in) {
    std::string out;
    for (unsigned char c : in) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += (char)c;
        }
    }
    return out;
}
}

std::string formatValue(FeatureType type, const uint8_t *data, size_t len) {
    char buf[32];
    switch (type) {
        case FeatureType::Bool:
            return (len >= 1 && data[0]) ? "true" : "false";
        case FeatureType::Int32: {
            if (len < 4) break;
            int32_t v;
            memcpy(&v, data, 4);
            snprintf(buf, sizeof(buf), "%ld", (long)v);
            return buf;
        }
        case FeatureType::Float: {
            if (len < 4) break;
            float v;
            memcpy(&v, data, 4);
            snprintf(buf, sizeof(buf), "%g", (double)v);
            return buf;
        }
        case FeatureType::String:
            return std::string((const char *)data, len);
        case FeatureType::KeyCombo: {
            if (len < 1) break;
            std::string out;
            if (data[0] & KEYMOD_CTRL) out += "Ctrl+";
            if (data[0] & KEYMOD_SHIFT) out += "Shift+";
            if (data[0] & KEYMOD_ALT) out += "Alt+";
            if (data[0] & KEYMOD_GUI) out += "Gui+";
            for (size_t i = 1; i < len; i++) {
                snprintf(buf, sizeof(buf), "%s0x%02X", i > 1 ? "+" : "", data[i]);
                out += buf;
            }
            return out;
        }
    }
    return "<invalid>";
}

std::string DeviceRegistry::macToString(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

bool DeviceRegistry::update(const uint8_t mac[6], const AnnounceMessage &msg, unsigned long nowMs) {
    return upsert(mac, msg, nowMs, true);
}

bool DeviceRegistry::restore(const uint8_t mac[6], const AnnounceMessage &msg) {
    return upsert(mac, msg, 0, false);
}

bool DeviceRegistry::upsert(const uint8_t mac[6], const AnnounceMessage &msg, unsigned long nowMs, bool online) {
    if (msg.feature_count > MANDOS_MAX_FEATURES) {
        return false;
    }

    Device device;
    device.mac = macToString(mac);
    memcpy(device.mac_raw.data(), mac, 6);
    device.name = boundedString(msg.name, sizeof(msg.name));
    device.last_seen_ms = nowMs;
    device.online = online;
    for (uint8_t i = 0; i < msg.feature_count; i++) {
        const FeatureInfo &f = msg.features[i];
        if (!validFeatureType(f.type)) {
            return false;
        }
        DeviceCapability cap;
        cap.name = boundedString(f.name, sizeof(f.name));
        cap.type = static_cast<FeatureType>(f.type);
        cap.can_read = f.flags & FEATURE_READ;
        cap.can_write = f.flags & FEATURE_WRITE;
        cap.can_notify = f.flags & FEATURE_NOTIFY;
        device.capabilities.push_back(cap);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(device.mac);
    if (it == devices_.end() || !sameContent(it->second, device)) {
        dirty_ = dirty_ || online;
    }
    devices_[device.mac] = std::move(device);
    return true;
}

void DeviceRegistry::markSeen(const uint8_t mac[6], unsigned long nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(macToString(mac));
    if (it != devices_.end()) {
        it->second.online = true;
        it->second.last_seen_ms = nowMs;
    }
}

void DeviceRegistry::setReachable(const uint8_t mac[6], bool reachable, unsigned long nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(macToString(mac));
    if (it == devices_.end()) {
        return;
    }
    it->second.online = reachable;
    if (reachable) {
        it->second.last_seen_ms = nowMs;
    }
}

std::vector<std::array<uint8_t, 6>> DeviceRegistry::offlineMacs() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::array<uint8_t, 6>> out;
    for (const auto &entry : devices_) {
        if (!entry.second.online) {
            out.push_back(entry.second.mac_raw);
        }
    }
    return out;
}

bool DeviceRegistry::takeDirty() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool d = dirty_;
    dirty_ = false;
    return d;
}

std::vector<StoredDevice> DeviceRegistry::exportAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredDevice> out;
    for (const auto &entry : devices_) {
        const Device &d = entry.second;
        StoredDevice s;
        s.mac = d.mac_raw;
        memset(&s.announce, 0, sizeof(s.announce));
        s.announce.header = makeHeader(MsgType::Announce);
        strncpy(s.announce.name, d.name.c_str(), sizeof(s.announce.name) - 1);
        s.announce.feature_count = d.capabilities.size();
        for (size_t i = 0; i < d.capabilities.size(); i++) {
            const DeviceCapability &c = d.capabilities[i];
            FeatureInfo &f = s.announce.features[i];
            strncpy(f.name, c.name.c_str(), sizeof(f.name) - 1);
            f.type = static_cast<uint8_t>(c.type);
            f.flags = (c.can_read ? FEATURE_READ : 0) | (c.can_write ? FEATURE_WRITE : 0) |
                      (c.can_notify ? FEATURE_NOTIFY : 0);
        }
        out.push_back(s);
    }
    return out;
}

std::string DeviceRegistry::describeValue(const uint8_t mac[6], const ValueMessage &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(macToString(mac));
    if (it == devices_.end() || msg.feature_index >= it->second.capabilities.size()) {
        return "";
    }
    const Device &d = it->second;
    const DeviceCapability &cap = d.capabilities[msg.feature_index];
    return d.mac + "  " + d.name + "/" + cap.name + " = " +
           formatValue(cap.type, msg.data, msg.length);
}

bool DeviceRegistry::checkAccess(const uint8_t mac[6], uint8_t featureIndex, uint8_t requiredFlags, int valueLen) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(macToString(mac));
    if (it == devices_.end() || featureIndex >= it->second.capabilities.size()) {
        return false;
    }
    const DeviceCapability &cap = it->second.capabilities[featureIndex];
    uint8_t flags = (cap.can_read ? FEATURE_READ : 0) | (cap.can_write ? FEATURE_WRITE : 0) |
                    (cap.can_notify ? FEATURE_NOTIFY : 0);
    if ((flags & requiredFlags) != requiredFlags) {
        return false;
    }
    if (valueLen < 0) {
        return true;
    }
    if (valueLen > (int)MANDOS_MAX_VALUE) {
        return false;
    }
    switch (cap.type) {
        case FeatureType::Bool: return valueLen == 1;
        case FeatureType::Int32:
        case FeatureType::Float: return valueLen == 4;
        case FeatureType::KeyCombo: return valueLen >= 1;
        case FeatureType::String: return true;
    }
    return false;
}

std::vector<Device> DeviceRegistry::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Device> out;
    for (const auto &entry : devices_) {
        out.push_back(entry.second);
    }
    return out;
}

size_t DeviceRegistry::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

std::string DeviceRegistry::dump() {
    std::lock_guard<std::mutex> lock(mutex_);
    char buf[96];
    snprintf(buf, sizeof(buf), "--- Registry: %u device(s) ---\n", (unsigned)devices_.size());
    std::string out = buf;
    for (const auto &entry : devices_) {
        const Device &d = entry.second;
        snprintf(buf, sizeof(buf), "%s  name=%s  %s  last_seen=%lums\n",
                 d.mac.c_str(), d.name.c_str(), d.online ? "online" : "offline", d.last_seen_ms);
        out += buf;
        for (const auto &c : d.capabilities) {
            snprintf(buf, sizeof(buf), "    - %s (%s) %s%s%s\n", c.name.c_str(),
                     featureTypeName(c.type), c.can_read ? "R" : "-",
                     c.can_write ? "W" : "-", c.can_notify ? "N" : "-");
            out += buf;
        }
    }
    return out;
}

std::string DeviceRegistry::toJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string out = "[";
    bool firstDev = true;
    for (const auto &entry : devices_) {
        const Device &d = entry.second;
        if (!firstDev) out += ",";
        firstDev = false;
        out += "{\"id\":\"" + d.mac + "\",\"name\":\"" + jsonEscape(d.name) + "\",\"online\":" +
               (d.online ? "true" : "false") + ",\"features\":[";
        bool firstFeat = true;
        for (const auto &c : d.capabilities) {
            if (!firstFeat) out += ",";
            firstFeat = false;
            std::string flags = std::string(c.can_read ? "R" : "") + (c.can_write ? "W" : "") +
                                (c.can_notify ? "N" : "");
            out += "{\"name\":\"" + jsonEscape(c.name) + "\",\"type\":\"" +
                   featureTypeName(c.type) + "\",\"flags\":\"" + flags + "\"}";
        }
        out += "]}";
    }
    out += "]";
    return out;
}
