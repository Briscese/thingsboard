#include "Advertisements.h"
#include <sstream>
#include <HTTPUpdate.h>
#include <cmath>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <Arduino.h>
#include "Distributor.h"
#include "esp_log.h"
#include <BLEScan.h>
#include <BLEDevice.h>

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF000000)>>24) + (((x)&0x00FF0000)>>8) + ((((x)&0xFF00)<<8) + (((x)&0xFF)<<24)))
extern Distributor* distributor;

float GetBattery(float batteryVoltage)
{
    float percentage = 0;
    int range = 0;

    if (batteryVoltage > 3.0f) range = 5;
    else if (batteryVoltage > 2.9f) range = 4;
    else if (batteryVoltage > 2.74f) range = 3;
    else if (batteryVoltage > 2.44f) range = 2;
    else if (batteryVoltage > 2.1f) range = 1;
    else range = 0;

    switch (range)
    {
        case 5:
            percentage = 100;
            break;
        case 4:
            percentage = 100 - (3.0f - batteryVoltage) * (58.0f / 0.1f);
            break;
        case 3:
            percentage = 42 - (2.9f - batteryVoltage) * (24.0f / 0.16f);
            break;
        case 2:
            percentage = 18 - (2.74f - batteryVoltage) * (12.0f / 0.3f);
            break;
        case 1:
            percentage = 6 - (2.44f - batteryVoltage) * (6.0f / 0.34f);
            break;
        case 0:
            percentage = 0;
            break;
    }

    return std::ceil(percentage);
}

float GetAccelerometer(uint8_t msb, uint8_t lsb) {
    uint16_t rawData = (msb << 8) | lsb;

    int integerPart = (rawData >> 8) & 0xFF;
    int fractionalPart = rawData & 0xFF;

    float fractionalValue = fractionalPart / 256.0;

    float result = integerPart + fractionalValue;

    if (rawData & 0x8000) {
        result -= 256.0;
    }

    return result;
}

void Advertisements::processIBeacon() {
    if(!device.haveManufacturerData()) return;

    std::string manufacturerData = device.getManufacturerData();
    uint8_t cManufacturerData[100];
    manufacturerData.copy((char*) cManufacturerData, manufacturerData.length(), 0);
    
    if(manufacturerData.length() == 25){
        Serial.printf("\n📦 Raw Data: ");
        for (size_t i = 0; i < manufacturerData.length(); i++) {
            Serial.printf("%02X ", cManufacturerData[i]);   
        }
        Serial.println();
        
        Serial.printf("🔍 Device Found: %s\n", device.toString().c_str());
        Serial.printf("📶 RSSI: %d dBm\n", device.getRSSI());
        
        BLEBeacon oBeacon = BLEBeacon();
        oBeacon.setData(manufacturerData);
        
        Serial.printf("\n🏷️  iBeacon Information:\n");
        Serial.printf("🆔 ID: %04X\n", oBeacon.getManufacturerId());
        Serial.printf("📍 Major: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMajor()));
        Serial.printf("📍 Minor: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMinor()));
        Serial.printf("🔑 UUID: %s\n", oBeacon.getProximityUUID().toString().c_str());
        Serial.printf("⚡ Power: %d dBm\n", oBeacon.getSignalPower());
        Serial.println();
        
        deviceType = 1;
    }
}

void Advertisements::processTelemetry() {
    if(!device.haveServiceData()) return;

    std::string serviceData = device.getServiceData();
    uint8_t data[100];
    serviceData.copy((char*)data, serviceData.length(), 0);
    
    if (device.getServiceDataUUID().equals(BLEUUID((uint16_t)0xFEAA))) {
        Serial.printf("\n📦 Raw Data: ");
        for (size_t i = 0; i < serviceData.length(); i++) {
            Serial.printf("%02X ", data[i]);   
        }
        Serial.println();
        
        Serial.printf("🔍 Device: %s\n", device.toString().c_str());
        Serial.printf("📶 RSSI: %d dBm\n", device.getRSSI());
        
        if (data[0] == 0x20) {
            uint8_t version = data[1];
            uint16_t batteryVoltage = (data[2] << 8) | data[3];
            int16_t temperature = (data[4] << 8) | data[5];
            uint32_t packetCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
            uint32_t uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];
            
            Serial.printf("\n📊 Telemetry:\n");
            Serial.printf("📱 Version: %d\n", version);
            Serial.printf("🔋 Battery: %.2fV (%.1f%%)\n", (float)batteryVoltage / 1000, GetBattery((float)batteryVoltage / 1000));
            Serial.printf("🌡️ Temperature: %.2f°C\n", temperature / 256.0);
            Serial.printf("📝 Packets: %u\n", packetCount);
            Serial.printf("⏱️ Active Time: %.1f days\n", (uptime / 10.0) / 86400);
            Serial.println();
            
            batteryLevel = GetBattery((float)batteryVoltage / 1000);
            timeActivity = (uptime / 10.0) / 86400;
        }
    }
}

void Advertisements::processAccelerometer() {
    if(!device.haveServiceData()) return;

    std::string deviceStr = device.toString();
    std::string macTemp = deviceStr.substr(deviceStr.find("Address: ")+21, 5);
    macTemp = macTemp.substr(0,2) + macTemp.substr(3,2);
    std::transform(macTemp.begin(), macTemp.end(), macTemp.begin(), ::toupper);

    std::string serviceData = device.getServiceData();
    uint8_t data[100];
    serviceData.copy((char*)data, serviceData.length(), 0);
    
    if (data[0] == 0xA1 && device.getServiceDataUUID().equals(BLEUUID((uint16_t)0xFFE1))) {
        Serial.printf("\n📦 Raw Data: ");
        for (size_t i = 0; i < serviceData.length(); i++) {
            Serial.printf("%02X ", data[i]);   
        }
        Serial.println();
        
        Serial.printf("🔍 Minew Device: %s\n", device.toString().c_str());
        Serial.printf("📶 RSSI: %d dBm\n", device.getRSSI());
        
        uint8_t version = data[1];
        batteryLevel = data[2];
        x = GetAccelerometer(data[3], data[4]);
        y = GetAccelerometer(data[5], data[6]);
        z = GetAccelerometer(data[7], data[8]);
        
        Serial.printf("\n📱 Version: %d\n", version);
        Serial.printf("🔋 Battery: %d%%\n", batteryLevel);
        Serial.printf("🎯 Accelerometer:\n");
        Serial.printf("  ➡️ X: %.2f\n", x);
        Serial.printf("  ⬆️ Y: %.2f\n", y);
        Serial.printf("  ↗️ Z: %.2f\n", z);
        Serial.println();
        
        deviceType = 4;
        deviceCode = macTemp;
        if (distributor != nullptr)
            distributor->UserRegisterData(macAddress.c_str(), deviceCode.c_str(), rssi, deviceType, batteryLevel, x, y, z, timeActivity);
    }
} 

void Advertisements::ListDevices(BLEScanResults foundDevices)
{
    int deviceCount = foundDevices.getCount();
    Serial.printf("Devices Found: %d\n", deviceCount);

    for (uint32_t i = 0; i < deviceCount; i++)
    {
        BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
        String deviceStr = advertisedDevice.toString().c_str();
        String macAddressDevice = deviceStr.substring(deviceStr.indexOf("Address: ")+9, deviceStr.indexOf("Address: ")+26);
        macAddressDevice.toUpperCase();

        setDevice(advertisedDevice);
        setMacAddress(macAddressDevice.c_str());
        setPayload(advertisedDevice.getPayload(), advertisedDevice.getPayloadLength());
        setRssi(advertisedDevice.getRSSI());

        processIBeacon();
        processTelemetry();
        processAccelerometer();
    }
}