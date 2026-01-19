#include "Advertisements.h"
#include <sstream>
#include <HTTPUpdate.h>
#include <cmath>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <Arduino.h>
#include "Distributor.h"
#include "Connect.h"
#include <ArduinoJson.h>
#include "esp_log.h"
#include <BLEScan.h>
#include <BLEDevice.h>
#include <deque>
#include <vector>
#include <iostream>

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
        Serial.printf("\n📦 Raw Data: ");
        for (size_t i = 0; i < len; i++)
        {
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

void Advertisements::processTelemetry(uint8_t *data, size_t len)
{
    if (len < 14)
        return;
    Serial.printf("\n🔍 Device: %s\n", macAddress.c_str());
    uint8_t version = data[1];
    uint16_t batteryVoltage = (data[2] << 8) | data[3];
    int16_t temperature = (data[4] << 8) | data[5];
    uint32_t packetCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
    uint32_t uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];

    Serial.printf("\n📊 Telemetry:\n");
    Serial.printf("📱 Version: %d\n", version);
    // ⚠️ Avoid float formatting in sprintf - causes malloc/dtoa crash
    uint16_t batteryMV = batteryVoltage;
    uint8_t batteryPercent = GetBattery((float)batteryVoltage / 1000);
    Serial.printf("🔋 Battery: %u mV (%d%%)\n", batteryMV, batteryPercent);
    int16_t tempRaw = temperature;
    Serial.printf("🌡️ Temperature: %d (raw)\n", tempRaw);
    Serial.printf("📝 Packets: %u\n", packetCount);
    Serial.printf("⏱️ Uptime: %u (×10ms)\n", uptime);
    Serial.println();

    batteryLevel = batteryPercent;
    timeActivity = (uptime / 10.0) / 86400;
    
    // 📡 Enviar dados de telemetria para ThingsBoard
    if (connect != nullptr && connect->isMQTTConnected()) {
        DynamicJsonDocument doc(256);
        String macStr(macAddress.c_str());
        doc["deviceId"] = macStr;
        doc["mac"] = macStr;
        doc["battery"] = batteryPercent;
        doc["batteryMV"] = batteryMV;
        doc["temperature"] = tempRaw;
        doc["packetCount"] = packetCount;
        doc["uptime"] = uptime;
        doc["rssi"] = rssi;
        doc["deviceType"] = 3;
        doc["timestamp"] = millis();
        
        String jsonData;
        serializeJson(doc, jsonData);
        
        if (!jsonData.isEmpty() && jsonData != "{}") {
            if (connect->publishTelemetry(jsonData)) {
                Serial.println("✅ Dados de telemetria enviados ao ThingsBoard!");
            } else {
                Serial.println("❌ Falha ao enviar dados de telemetria");
            }
        }
    }
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
            Serial.printf("❌ Invalid offset[%d]=%d for len=%u\n", i, offset[i], len);
            return;
        }
    }

    size_t maxIdx = *std::max_element(offset.begin(), offset.end());
    if (len <= maxIdx)
        return;

    Serial.printf("🔍 Minew Device: %s\n", macAddress.c_str());

    uint8_t version = data[offset[0]];
    batteryLevel = data[offset[1]];
    x = GetAccelerometer(data[offset[2]], data[offset[3]]);
    y = GetAccelerometer(data[offset[4]], data[offset[5]]);
    z = GetAccelerometer(data[offset[6]], data[offset[7]]);

    Serial.printf("\n📱 Version: %d\n", version);
    Serial.printf("🔋 Battery: %d%%\n", batteryLevel);
    Serial.printf("🎯 Accelerometer:\n");
    Serial.printf("  ➡️ X: %.2f\n", x);
    Serial.printf("  ⬆️ Y: %.2f\n", y);
    Serial.printf("  ↗️ Z: %.2f\n", z);
    Serial.println();

    deviceType = 4;
    deviceCode = macAddress;  // ✅ Usar macAddress diretamente (não temporário)
    if (device.haveName())
        name = device.getName().c_str();
    else
        name = "Placa desconhecida nº ";
    
    if (distributor != nullptr) {
        distributor->UserRegisterData(macAddress.c_str(), deviceCode.c_str(), rssi, deviceType, batteryLevel, x, y, z, timeActivity, name);
        
        // 📡 Enviar dados de acelerômetro para ThingsBoard imediatamente
        if (connect != nullptr && connect->isMQTTConnected()) {
            DynamicJsonDocument doc(256);
            String macStr(macAddress.c_str());
            doc["deviceId"] = macStr;
            doc["mac"] = macStr;
            doc["battery"] = batteryLevel;
            doc["x"] = roundf(x * 1000) / 1000;
            doc["y"] = roundf(y * 1000) / 1000;
            doc["z"] = roundf(z * 1000) / 1000;
            doc["rssi"] = rssi;
            doc["deviceType"] = deviceType;
            doc["timestamp"] = millis();
            
            String jsonData;
            serializeJson(doc, jsonData);
            
            if (!jsonData.isEmpty() && jsonData != "{}") {
                if (connect->publishTelemetry(jsonData)) {
                    Serial.println("✅ Dados de acelerômetro enviados ao ThingsBoard!");
                } else {
                    Serial.println("❌ Falha ao enviar dados de acelerômetro");
                }
            }
        }
    }
}

void Advertisements::ListDevices(BLEScanResults foundDevices)
{
    int maxDevices = foundDevices.getCount();
    if (maxDevices <= 0)
        return;

    Serial.printf("Devices Found: %d\n", maxDevices);

    // ⚠️ SAFETY: limit to avoid stack/memory issues when many devices are present
    int deviceCount = maxDevices;
    if (deviceCount > 100)
    {
        Serial.printf("⚠️  Device limit: Processing max 100 of %d\n", deviceCount);
        deviceCount = 100;
    }

    for (int i = 0; i < deviceCount; i++)
    {
        try
        {
            BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);

            // ✅ MAC: Use API directly - returns String (Arduino)
            String macStr = advertisedDevice.getAddress().toString();
            macStr.toUpperCase();
            setMacAddress(macStr.c_str());

            setRssi(advertisedDevice.getRSSI());

            // ✅ Only process if has Service Data
            if (!advertisedDevice.haveServiceData())
            {
                continue;
            }

            BLEUUID serviceUUID = advertisedDevice.getServiceDataUUID();

            // ✅ Service Data BINARY (correct way - String from Arduino lib)
            String sd = advertisedDevice.getServiceData();
            size_t len = sd.length();
            
            if (!len || len > 64)
            {
                // 64 is reasonable limit to avoid garbage; adjust if needed
                continue;
            }
            
            // 🔧 CRITICAL: Copy binary data to local buffer IMMEDIATELY
            // (String.c_str() pointer becomes invalid after String lifecycle ends)
            uint8_t data[64];
            memcpy(data, sd.c_str(), len);

            // 📊 Log: Show UUID, size, type, and full hex
            Serial.printf("\n📦 Service Data UUID: %s | Size: %u bytes | Type: 0x%02X | Data: ",
                          serviceUUID.toString().c_str(), (unsigned)len, data[0]);
            for (size_t j = 0; j < len; j++)
                Serial.printf("%02X ", data[j]);
            Serial.printf("\n📶 RSSI: %d dBm\n", advertisedDevice.getRSSI());

            // ✅ Match against table
            for (const mapData &item : list)
            {

                if (!serviceUUID.equals(BLEUUID((uint16_t)item.uuid)))
                {
                    continue;
                }

                // If accelerometer (Minew/Moko): validate type and minimum size by offsets
                if (!item.offset.empty())
                {

                    if (len < 1)
                        continue;

                    if (data[0] != item.type)
                    {
                        Serial.printf("❌ Tipo mismatch: esperado 0x%02X, recebido 0x%02X\n", item.type, data[0]);
                        continue;
                    }

                    size_t minRequired = (size_t)(*std::max_element(item.offset.begin(), item.offset.end()) + 1);
                    if (len < minRequired)
                    {
                        Serial.printf("⚠️  Pacote curto: %u bytes, mínimo %u bytes para UUID 0x%04X\n",
                                      (unsigned)len, (unsigned)minRequired, item.uuid);
                        continue;
                    }

                    Serial.printf("✅ Processando ACCELEROMETER (UUID 0x%04X, Tipo 0x%02X): %u bytes\n",
                                  item.uuid, item.type, (unsigned)len);

                    // Safe cast (processAccelerometer expects uint8_t*)
                    processAccelerometer((uint8_t *)data, len, item.offset);

                }
                else
                {
                    // Telemetry (Eddystone TLM normally 0x20, with 14 bytes)
                    if (len >= 14)
                    {
                        Serial.printf("✅ Processando TELEMETRY (UUID 0x%04X): %u bytes\n",
                                      item.uuid, (unsigned)len);
                        processTelemetry((uint8_t *)data, len);
                    }
                    else
                    {
                        Serial.printf("⚠️  Telemetria curta: %u bytes (mínimo 14)\n", (unsigned)len);
                    }
                }
            }
        }
        catch (...)
        {
            Serial.printf("❌ Exception processing device %d\n", i);
            continue;
        }
    }
    Serial.printf("✅ ListDevices() completou com sucesso!\n");
}
