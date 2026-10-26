#include "KTD2052.h"

namespace {
constexpr uint8_t REG_ID = 0x00;
constexpr uint8_t REG_MONITOR = 0x01;
constexpr uint8_t REG_CONTROL = 0x02;
constexpr uint8_t REG_RED1 = 0x03;
constexpr uint8_t REG_PG_CONTROL = 0x0F;
}

KTD2052::KTD2052(TwoWire& wire, uint8_t address)
    : _wire(wire), _address(address) {}

bool KTD2052::isConnected() {
    _wire.beginTransmission(_address);
    return _wire.endTransmission() == 0;
}

bool KTD2052::writeBurst(uint8_t reg, const uint8_t* values, size_t count) {
    _wire.beginTransmission(_address);
    _wire.write(reg);
    for (size_t i = 0; i < count; ++i) {
        _wire.write(values[i]);
    }
    return _wire.endTransmission() == 0;
}

bool KTD2052::writeRegister(uint8_t reg, uint8_t value) {
    return writeBurst(reg, &value, 1);
}

bool KTD2052::readRegister(uint8_t reg, uint8_t& value) {
    _wire.beginTransmission(_address);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) return false; // repeated START
    if (_wire.requestFrom(_address, static_cast<uint8_t>(1)) != 1) return false;
    value = static_cast<uint8_t>(_wire.read());
    return true;
}

bool KTD2052::getDeviceId(uint8_t& id) { return readRegister(REG_ID, id); }
bool KTD2052::getStatus(uint8_t& status) { return readRegister(REG_MONITOR, status); }

bool KTD2052::reset() {
    // CONTROL.EN_MODE=11 resets all registers to their silicon defaults.
    if (!writeRegister(REG_CONTROL, 0xC0)) return false;
    _control = 0x80;
    return true;
}

bool KTD2052::begin(Mode mode) {
    uint8_t id = 0;
    if (!getDeviceId(id)) return false;
    if ((id & 0xE0) != 0xA0) return false; // vendor ID 101b
    if (!reset()) return false;
    // Disable built-in AutoBreathe/pattern generator, for B/D variants too.
    if (!writeRegister(REG_PG_CONTROL, 0x00)) return false;
    if (!allOff()) return false;
    _control = static_cast<uint8_t>(static_cast<uint8_t>(mode) << 6);
    return writeRegister(REG_CONTROL, _control);
}

bool KTD2052::updateControl(uint8_t mask, uint8_t bits) {
    const uint8_t updated = (_control & static_cast<uint8_t>(~mask)) | (bits & mask);
    if (!writeRegister(REG_CONTROL, updated)) return false;
    _control = updated;
    return true;
}

bool KTD2052::setMode(Mode mode) {
    if (static_cast<uint8_t>(mode) > 2) return false;
    return updateControl(0xC0, static_cast<uint8_t>(static_cast<uint8_t>(mode) << 6));
}

bool KTD2052::setFadeRate(FadeRate rate) {
    if (static_cast<uint8_t>(rate) > 7) return false;
    return updateControl(0x07, static_cast<uint8_t>(rate));
}

bool KTD2052::setBrightExtend(bool enabled) {
    return updateControl(0x20, enabled ? 0x20 : 0);
}

bool KTD2052::setCoolExtend(uint8_t temperatureIndex) {
    if (temperatureIndex > 3) return false;
    return updateControl(0x18, static_cast<uint8_t>(temperatureIndex << 3));
}

uint8_t KTD2052::clampCurrent(uint8_t value) {
    return value > MAX_CURRENT ? MAX_CURRENT : value;
}

bool KTD2052::setRGB(uint8_t led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led < 1 || led > 4) return false;
    const uint8_t values[3] = {
        clampCurrent(red), clampCurrent(green), clampCurrent(blue)
    };
    return writeBurst(static_cast<uint8_t>(REG_RED1 + (led - 1) * 3), values, 3);
}

bool KTD2052::setChannel(uint8_t led, uint8_t channel, uint8_t level) {
    if (led < 1 || led > 4 || channel > 2) return false;
    return writeRegister(static_cast<uint8_t>(REG_RED1 + (led - 1) * 3 + channel),
                         clampCurrent(level));
}

bool KTD2052::allOff() {
    const uint8_t zeros[12] = {};
    return writeBurst(REG_RED1, zeros, sizeof(zeros));
}
