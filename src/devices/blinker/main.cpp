#include <Arduino.h>
#include <mandos_device.h>

// Blinker: blinks the onboard LED (GPIO 8, active low on the C3 Super Mini).
// The "frequency" feature is the blink interval in milliseconds (int32, little endian).
constexpr uint8_t LED_PIN = 8;
constexpr int32_t DEFAULT_INTERVAL_MS = 500;
constexpr int32_t MIN_INTERVAL_MS = 20;
constexpr int32_t MAX_INTERVAL_MS = 60000;

class FrequencyFeature : public Feature {
public:
    FrequencyFeature() : Feature("frequency", FeatureType::Int32, FEATURE_READ | FEATURE_WRITE) {}

    bool onWrite(const uint8_t *data, size_t len) override {
        if (len != sizeof(int32_t)) return false;
        int32_t v;
        memcpy(&v, data, sizeof(v));
        if (v < MIN_INTERVAL_MS || v > MAX_INTERVAL_MS) return false;
        intervalMs_ = v;
        return true;
    }

    size_t onRead(uint8_t *out, size_t max) override {
        if (max < sizeof(int32_t)) return 0;
        int32_t v = intervalMs_;
        memcpy(out, &v, sizeof(v));
        return sizeof(v);
    }

    int32_t intervalMs() const { return intervalMs_; }

private:
    // Written from the WiFi task, read from loop()
    volatile int32_t intervalMs_ = DEFAULT_INTERVAL_MS;
};

MandosDevice dev("blinker");
FrequencyFeature frequency;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    dev.addFeature(frequency);
    dev.begin();
}

void loop() {
    dev.loop();

    static unsigned long lastToggle = 0;
    static bool on = false;
    if (millis() - lastToggle >= (unsigned long)frequency.intervalMs()) {
        lastToggle = millis();
        on = !on;
        digitalWrite(LED_PIN, on ? LOW : HIGH);
    }
}
