#include "registry_store.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace core_store {

namespace {
constexpr const char *kPath = "/registry.bin";
constexpr const char *kTmpPath = "/registry.tmp";
constexpr uint8_t kFormat = 1;

// File layout: [format:1][count:1] then count x [mac:6][AnnounceMessage].
bool mount() {
    static bool mounted = false;
    if (!mounted) {
        mounted = LittleFS.begin(true);
        if (!mounted) {
            Serial.println("LittleFS mount failed");
        }
    }
    return mounted;
}
}

size_t load(DeviceRegistry &registry) {
    if (!mount() || !LittleFS.exists(kPath)) {
        return 0;
    }
    File f = LittleFS.open(kPath, "r");
    if (!f) {
        return 0;
    }

    uint8_t head[2];
    if (f.read(head, 2) != 2 || head[0] != kFormat) {
        return 0;
    }

    size_t restored = 0;
    for (uint8_t i = 0; i < head[1]; i++) {
        uint8_t mac[6];
        AnnounceMessage msg;
        if (f.read(mac, 6) != 6 || f.read((uint8_t *)&msg, sizeof(msg)) != sizeof(msg)) {
            break;
        }
        if (registry.restore(mac, msg)) {
            restored++;
        }
    }
    Serial.printf("Restored %u device(s) from flash\n", (unsigned)restored);
    return restored;
}

bool save(DeviceRegistry &registry) {
    if (!mount()) {
        return false;
    }
    std::vector<StoredDevice> devices = registry.exportAll();

    // Write to a temp file first so a power loss never corrupts the saved registry.
    File f = LittleFS.open(kTmpPath, "w");
    if (!f) {
        return false;
    }
    uint8_t head[2] = {kFormat, (uint8_t)devices.size()};
    bool ok = f.write(head, 2) == 2;
    for (const auto &d : devices) {
        ok = ok && f.write(d.mac.data(), 6) == 6 &&
             f.write((const uint8_t *)&d.announce, sizeof(d.announce)) == sizeof(d.announce);
    }
    f.close();

    if (!ok) {
        LittleFS.remove(kTmpPath);
        return false;
    }
    LittleFS.remove(kPath);
    return LittleFS.rename(kTmpPath, kPath);
}

}
