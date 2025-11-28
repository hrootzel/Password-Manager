#ifndef BLESERVICE_H
#define BLESERVICE_H

#include <Arduino.h>
#include <BleKeyboard.h>
#include <string>

class BleService {
public:
    BleService();
    void begin();
    void end();
    void sendString(const std::string& text);
    void sendChunkedString(const std::string& data, size_t chunkSize = 128, unsigned long delayBetweenChunks = 50);
    bool isReady() const;
    bool isConnected() const;
    void setLayout(const uint8_t* newLayout);
    void setDeviceName(const std::string& name);
    void clearBonds();

private:
    BleKeyboard keyboard;
    const uint8_t* layout;
    bool initialized = false;
    std::string deviceName;
};

#endif // BLESERVICE_H
