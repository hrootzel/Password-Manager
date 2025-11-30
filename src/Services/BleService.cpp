#include "BleService.h"

#if defined(USE_NIMBLE)
#include <NimBLEDevice.h>
#endif

BleService::BleService()
    : keyboard(), layout(KeyboardLayout_en_US), initialized(false), deviceName("Password Vault BLE") {}

void BleService::setLayout(const uint8_t* newLayout) {
    layout = newLayout;
    if (initialized) {
        keyboard.setLayout(layout);
    }
}

void BleService::setDeviceName(const std::string& name) {
    deviceName = name;
    if (!initialized) {
        keyboard.setName(String(deviceName.c_str()));
    }
}

void BleService::begin() {
    if (initialized) {
        return;
    }

    keyboard.setName(String(deviceName.c_str()));
    keyboard.begin(layout);
    initialized = true;
}

void BleService::end() {
    if (!initialized) {
        return;
    }

    keyboard.end();
    initialized = false;
}

bool BleService::isConnected() const {
    return keyboard.isConnected();
}

bool BleService::isReady() const {
    return initialized && keyboard.isConnected();
}

bool BleService::sendString(const std::string& text) {
    if (!initialized) {
        return false;
    }

#if defined(USE_NIMBLE)
    NimBLEDevice::startAdvertising(); // ensure we're discoverable if we lost the link
#endif

    const unsigned long connectionTimeoutMs = 500; // quick fallback if not connected
    unsigned long start = millis();
    while (!keyboard.isConnected() && (millis() - start < connectionTimeoutMs)) {
        delay(10);
    }

    if (!keyboard.isConnected()) {
        return false;
    }

    keyboard.releaseAll();
    for (const char& c : text) {
        keyboard.write(static_cast<uint8_t>(c));
        if (!keyboard.isConnected()) {
            break; // stop mid-stream if link drops
        }
    }
    return keyboard.isConnected();
}

void BleService::clearBonds() {
#if defined(USE_NIMBLE)
    NimBLEDevice::deleteAllBonds();
#endif
}

void BleService::sendChunkedString(const std::string& data, size_t chunkSize, unsigned long delayBetweenChunks) {
    size_t totalLength = data.length();
    size_t sentLength = 0;

    while (sentLength < totalLength) {
        size_t remainingLength = totalLength - sentLength;
        size_t currentChunkSize = (remainingLength > chunkSize) ? chunkSize : remainingLength;

        std::string chunk = data.substr(sentLength, currentChunkSize);
        if (!sendString(chunk)) {
            break;
        }

        sentLength += currentChunkSize;
        if (!keyboard.isConnected()) {
            break;
        }
        if (sentLength < totalLength) {
            delay(delayBetweenChunks);
        }
    }
}
