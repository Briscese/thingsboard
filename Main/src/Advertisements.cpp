#include "Advertisements.h"
#include <sstream>
#include <HTTPUpdate.h>
#include <cmath>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <Arduino.h>
#include <WiFi.h>
#include "Distributor.h"
#include "Connect.h"
#include "config/Config.h"
#include <ArduinoJson.h>
#include "esp_log.h"
#include <BLEScan.h>
#include <BLEDevice.h>
#include <deque>
#include <vector>
#include <iostream>
#include <cctype>

#define ENDIAN_CHANGE_U16(x) ((((x) & 0xFF000000) >> 24) + (((x) & 0x00FF0000) >> 8) + ((((x) & 0xFF00) << 8) + (((x) & 0xFF) << 24)))
extern Distributor *distributor;
extern Connect* connect;

std::vector<int> minewOffSet = {1, 2, 3, 4, 5, 6, 7, 8};
std::vector<int> mokoOffSet = {5, 12, 13, 6, 7, 8, 9, 10, 11};

struct mapData
{
    uint16_t uuid;
    uint8_t type;
    std::vector<int> offset;
};

std::vector<mapData> list = {
    {0xFEAA, 0x20, {}},
    {0xFFE1, 0xA1, minewOffSet},
    {0xFEAB, 0x60, mokoOffSet}};

float GetBattery(float batteryVoltage)
{
    float percentage = 0;
    int range = 0;

    if (batteryVoltage > 3.0f)
        range = 5;
    else if (batteryVoltage > 2.9f)
        range = 4;
    else if (batteryVoltage > 2.74f)
        range = 3;
    else if (batteryVoltage > 2.44f)
        range = 2;
    else if (batteryVoltage > 2.1f)
        range = 1;
    else
        range = 0;

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

float GetAccelerometer(uint8_t msb, uint8_t lsb)
{
    uint16_t rawData = (msb << 8) | lsb;

    int integerPart = (rawData >> 8) & 0xFF;
    int fractionalPart = rawData & 0xFF;

    float fractionalValue = fractionalPart / 256.0;

    float result = integerPart + fractionalValue;

    if (rawData & 0x8000)
    {
        result -= 256.0;
    }

    return result;
}

static std::string GetMacLast4(const std::string& mac) {
    std::string last4;
    last4.reserve(4);
    for (int i = static_cast<int>(mac.length()) - 1; i >= 0 && last4.length() < 4; --i) {
        char c = mac[static_cast<size_t>(i)];
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f')) {
            last4.insert(last4.begin(), c);
        }
    }
    if (last4.length() < 4) {
        return mac;
    }
    for (char& c : last4) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return last4;
}

static std::string NormalizeMac(const std::string& mac) {
    std::string normalized;
    normalized.reserve(12);
    for (char c : mac) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return normalized;
}

static String NormalizeMacString(const String& mac) {
    String normalized = "";
    normalized.reserve(12);
    for (size_t i = 0; i < mac.length(); i++) {
        char c = mac[i];
        if (isxdigit(static_cast<unsigned char>(c))) {
            normalized += static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
    }
    return normalized;
}

static double NormalizeAzimuth(double azimuth) {
    double normalized = fmod(azimuth, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

String Advertisements::getMacAddress()
{
    String deviceStr = device.toString();
    std::string deviceStd = std::string(deviceStr.c_str());
    size_t pos = deviceStd.find("Address: ");
    if (pos == std::string::npos || deviceStd.size() < pos + 17)
    {
        // Formato inesperado, evita accesso fora de faixa
        return String("");
    }

    // Exemplo de trecho: "Address: aa:bb:cc:dd:ee:ff"
    std::string macRaw = deviceStd.substr(pos + 9, 17); // aa:bb:cc:dd:ee:ff
    // Remove ':'
    std::string macTemp;
    macTemp.reserve(12);
    for (char c : macRaw)
    {
        if (c != ':')
            macTemp.push_back(c);
    }
    std::transform(macTemp.begin(), macTemp.end(), macTemp.begin(), ::toupper);
    return macTemp.c_str();
}

void Advertisements::processIBeacon()
{
    if (!device.haveManufacturerData())
        return;

    String manufacturerData = device.getManufacturerData();
    uint8_t cManufacturerData[100];
    std::string stdManufacturerData = std::string(manufacturerData.c_str());
    size_t len = std::min(stdManufacturerData.length(), sizeof(cManufacturerData) - 1);
    stdManufacturerData.copy((char *)cManufacturerData, len, 0);

    if (len == 25)
    {
        BLEBeacon oBeacon = BLEBeacon();
        oBeacon.setData(manufacturerData);
        deviceType = 1;
    }
}

void Advertisements::processTelemetry(uint8_t *data, size_t len)
{
    if (len < 14)
        return;
    
    // ⚠️ Verificar se macAddress foi inicializado
    if (macAddress.empty()) {
        Serial.println("❌ ERRO: macAddress não inicializado em processTelemetry!");
        return;
    }
    
    uint8_t version = data[1];
    uint16_t batteryVoltage = (data[2] << 8) | data[3];
    int16_t temperature = (data[4] << 8) | data[5];
    uint32_t packetCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
    uint32_t uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];

    uint16_t batteryMV = batteryVoltage;
    uint8_t batteryPercent = GetBattery((float)batteryVoltage / 1000);
    int16_t tempRaw = temperature;
    float temperatureC = static_cast<int8_t>(data[4]) + (static_cast<float>(data[5]) / 256.0f);

    batteryLevel = batteryPercent;
    timeActivity = (uptime / 10.0) / 86400;

    if (distributor != nullptr) {
        distributor->updateTrackedBatteryLevel(String(macAddress.c_str()), batteryLevel);
    }
    
    // Telemetria MQTT secundaria desabilitada.
    // O envio MQTT foi centralizado em Distributor::publishTrackedMacRssiTelemetry.
}

void Advertisements::processAccelerometer(uint8_t *data, size_t len, const std::vector<int> &offset)
{
    if (offset.empty())
        return;
    if (offset.size() < 8)
        return;

    // ✅ Validate each offset before access
    for (int i = 0; i < 8; i++)
    {
        if (offset[i] < 0 || offset[i] >= (int)len)
        {
            return;
        }
    }

    size_t maxIdx = *std::max_element(offset.begin(), offset.end());
    if (len <= maxIdx)
        return;

    // ⚠️ Verificar se macAddress foi inicializado
    if (macAddress.empty()) {
        Serial.println("❌ ERRO: macAddress não inicializado em processAccelerometer!");
        return;
    }

    // Identificar vendor pela tabela de offsets
    const char* vendor = "unknown";
    if (offset == minewOffSet) {
        vendor = "minew";
    } else if (offset == mokoOffSet) {
        vendor = "moko";
    }

    // Somente MOKO: ignora Minew/unknown
    if (strcmp(vendor, "moko") != 0) {
        return;
    }

    uint8_t version = data[offset[0]];
    batteryLevel = data[offset[1]];
    x = GetAccelerometer(data[offset[2]], data[offset[3]]);
    y = GetAccelerometer(data[offset[4]], data[offset[5]]);
    z = GetAccelerometer(data[offset[6]], data[offset[7]]);

    deviceType = 4;
    deviceCode = GetMacLast4(macAddress);  // Usar os 4 últimos hex do MAC (ex: 595C)
    if (device.haveName())
        name = device.getName().c_str();
    else
        name = "Placa desconhecida nº ";
    
    if (distributor != nullptr) {
        distributor->UserRegisterData(macAddress.c_str(), deviceCode.c_str(), rssi, deviceType, batteryLevel, x, y, z, timeActivity, name);
        
        // Geolocalizacao MQTT desabilitada: envio centralizado em Distributor::publishTrackedMacRssiTelemetry.
    }
}

void Advertisements::ListDevices(BLEScanResults& foundDevices)
{
    int maxDevices = foundDevices.getCount();
    if (maxDevices <= 0)
        return;

    // Limitar busca para evitar crashes
    int searchLimit = min(maxDevices, 100);
    const int accelLimit = 30;
    int accelProcessed = 0;
    bool processedTrackedDevice = false;
    const String targetMacNormalized = NormalizeMacString(String(TARGET_BEACON_MAC));

    for (int i = 0; i < searchLimit; i++) {
            BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);

            // ✅ MAC: Use API directly - returns String (Arduino)
            String macStr = advertisedDevice.getAddress().toString();
            macStr.toUpperCase();

            bool shouldProcess = false;
            if (distributor != nullptr && distributor->hasTrackedMacs()) {
                shouldProcess = distributor->isTrackedMac(macStr);
            } else {
                shouldProcess = (NormalizeMacString(macStr) == targetMacNormalized);
            }

            if (!shouldProcess) {
                continue;
            }

            processedTrackedDevice = true;
            setMacAddress(macStr.c_str());
            setRssi(advertisedDevice.getRSSI());

            // ✅ Only process if has Service Data
            if (!advertisedDevice.haveServiceData()) {
                continue;
            }

            BLEUUID serviceUUID = advertisedDevice.getServiceDataUUID();

            // ✅ Service Data BINARY (correct way - String from Arduino lib)
            String sd = advertisedDevice.getServiceData();
            size_t len = sd.length();

            if (!len || len > 64) {
                continue;
            }

            // 🔧 CRITICAL: Copy binary data to local buffer IMMEDIATELY
            uint8_t data[64];
            memcpy(data, sd.c_str(), len);

            // ✅ Match against table
            for (const mapData &item : list) {
                if (!serviceUUID.equals(BLEUUID((uint16_t)item.uuid))) {
                    continue;
                }

                // If accelerometer (Minew/Moko): validate type and minimum size by offsets
                if (!item.offset.empty()) {
                    if (accelProcessed >= accelLimit) {
                        continue;
                    }

                    if (len < 1)
                        continue;

                    if (data[0] != item.type) {
                        continue;
                    }

                    size_t minRequired = (size_t)(*std::max_element(item.offset.begin(), item.offset.end()) + 1);
                    if (len < minRequired) {
                        continue;
                    }

                    processAccelerometer((uint8_t *)data, len, item.offset);
                    accelProcessed++;
                } else {
                    // Telemetry (Eddystone TLM normally 0x20, with 14 bytes)
                    if (len >= 14) {
                        processTelemetry((uint8_t *)data, len);
                    }
                }
            }
    }

    if (!processedTrackedDevice) {
        return;
    }
}
