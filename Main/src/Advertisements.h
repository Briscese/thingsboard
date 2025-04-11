#ifndef ADVERTISEMENTS_H
#define ADVERTISEMENTS_H

#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEScan.h>
#include <string>
#include <vector>

class Advertisements {
private:
    BLEAdvertisedDevice device;
    std::string macAddress;
    uint8_t* payload;
    size_t payloadLength;
    int rssi;
    int batteryLevel;
    float x;
    float y;
    float z;
    float timeActivity;
    int deviceType;
    std::string deviceCode;
    bool isTagPressed;
    bool isFindPressed;

public:
    Advertisements() {
        payload = nullptr;
        payloadLength = 0;
        rssi = 0;
        batteryLevel = 0;
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        timeActivity = 0.0f;
        deviceType = 0;
        isTagPressed = false;
        isFindPressed = false;
    }

    // Setters
    void setDevice(BLEAdvertisedDevice dev) { device = dev; }
    void setMacAddress(const std::string& mac) { macAddress = mac; }
    void setPayload(uint8_t* pl, size_t length) { 
        payload = pl; 
        payloadLength = length;
    }
    void setRssi(int value) { rssi = value; }
    void setBatteryLevel(int level) { batteryLevel = level; }
    void setCoordinates(float xVal, float yVal, float zVal) {
        x = xVal;
        y = yVal;
        z = zVal;
    }
    void setTimeActivity(float time) { timeActivity = time; }
    void setDeviceType(int type) { deviceType = type; }
    void setDeviceCode(const std::string& code) { deviceCode = code; }
    void setTagPressed(bool pressed) { isTagPressed = pressed; }
    void setFindPressed(bool pressed) { isFindPressed = pressed; }

    // Getters
    std::string getMacAddress() const { return macAddress; }
    uint8_t* getPayload() const { return payload; }
    size_t getPayloadLength() const { return payloadLength; }
    int getRssi() const { return rssi; }
    int getBatteryLevel() const { return batteryLevel; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getTimeActivity() const { return timeActivity; }
    int getDeviceType() const { return deviceType; }
    std::string getDeviceCode() const { return deviceCode; }
    bool getTagPressed() const { return isTagPressed; }
    bool getFindPressed() const { return isFindPressed; }

    void processIBeacon();
    void processTelemetry();
    void processAccelerometer();
    void ListDevices(BLEScanResults foundDevices);
};

#endif // ADVERTISEMENTS_H 