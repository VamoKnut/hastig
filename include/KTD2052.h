#pragma once

#include <Arduino.h>
#include <Wire.h>

// Small Arduino Wire driver for the Kinetic Technologies KTD2052.
// RGB index is 1..4, raw current is 0..192 (0..24mA in normal mode).
// No Wire.begin() is performed by this driver; the application owns the bus.
class KTD2052 {
public:
    static constexpr uint8_t ADDRESS_AB = 0x74;
    static constexpr uint8_t ADDRESS_CD = 0x75;
    static constexpr uint8_t MAX_CURRENT = 192;

    enum class Mode : uint8_t { Off = 0, Night = 1, Normal = 2 };
    enum class FadeRate : uint8_t {
        Ms32 = 0, Ms63 = 1, Ms125 = 2, Ms250 = 3,
        Ms500 = 4, Sec1 = 5, Sec2 = 6, Sec4 = 7
    };

    explicit KTD2052(TwoWire& wire = Wire, uint8_t address = ADDRESS_AB);

    // Configure chip for independent software-controlled RGB outputs.
    // Resets chip registers (including any previous pattern) and turns LEDs off.
    bool begin(Mode mode = Mode::Normal);
    bool isConnected();
    bool reset();
    bool setMode(Mode mode);
    bool setFadeRate(FadeRate rate);
    bool setBrightExtend(bool enabled);
    bool setCoolExtend(uint8_t temperatureIndex); // 0=135C, 1=120C, 2=105C, 3=90C

    // Raw levels: 0..192 correspond to 0..24mA normal mode and 0..1.5mA night mode.
    // Values >192 are clamped to 192. RGB indices 1..4 are valid.
    bool setRGB(uint8_t led, uint8_t red, uint8_t green, uint8_t blue);
    bool setChannel(uint8_t led, uint8_t channel, uint8_t level); // channel: 0=R, 1=G, 2=B
    bool allOff(); // Set all 12 current registers to zero; mode remains enabled.

    // Returns false on I2C failure. The status register bits 3:0 are:
    // bit 3 short circuit; bit 2 BrightExtend active; bit 1 CoolExtend active;
    // bit 0 undervoltage or overtemperature shutdown.
    bool getStatus(uint8_t& status);
    bool getDeviceId(uint8_t& id);

    // Advanced access, e.g. internal pattern generator at registers 0x0F..0x15.
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);

private:
    TwoWire& _wire;
    uint8_t _address;
    uint8_t _control = 0x80; // mode=normal, fade=32ms, optional features disabled
    bool writeBurst(uint8_t startReg, const uint8_t* values, size_t count);
    bool updateControl(uint8_t mask, uint8_t bits);
    static uint8_t clampCurrent(uint8_t value);
};
