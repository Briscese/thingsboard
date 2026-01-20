#include "Advertisements.h"
#include <sstream>
#include <HTTPUpdate.h>
#include <cmath>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <Arduino.h>
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
        BLEBeacon oBeacon = BLEBeacon();
        oBeacon.setData(manufacturerData);
        deviceType = 1;
    }
}

void Advertisements::processTelemetry(uint8_t *data, size_t len)
{
    if (len < 14)
        return;
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
    
    // 📡 Enviar dados de telemetria para ThingsBoard (buffer estático para evitar alocações)
    if (connect != nullptr && connect->isMQTTConnected()) {
        static DynamicJsonDocument doc(512);
        static char jsonBuf[512];
        doc.clear();

        doc["messageType"] = "telemetry_tlm";
        doc["gatewayId"] = DEVICE_ID;
        doc["deviceId"] = macAddress;
        doc["mac"] = macAddress;
        doc["battery"] = batteryPercent;
        doc["batteryMV"] = batteryMV;
        doc["temperatureRaw"] = tempRaw;
        doc["temperatureC"] = temperatureC;
        doc["packetCount"] = packetCount;
        doc["uptime"] = uptime;
        doc["rssi"] = rssi;
        doc["deviceType"] = 3;
        doc["ts"] = connect->getEpochMillis();

        size_t written = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
        if (written > 0 && written < sizeof(jsonBuf)) {
            connect->loopMQTT();
            if (connect->publishTelemetryRaw(jsonBuf, written)) {
                Serial.printf("✅ Telemetria enviada: %s\n", jsonBuf);
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

    // Identificar vendor pela tabela de offsets
    const char* vendor = "unknown";
    if (offset == minewOffSet) {
        vendor = "minew";
    } else if (offset == mokoOffSet) {
        vendor = "moko";
    }
    
    // ⏱️ Rate limiting: wait 500ms minimum between MQTT publishes to prevent heap exhaustion
    static unsigned long lastPublishTime = 0;
    unsigned long now = millis();
    if (now - lastPublishTime < 500) {
        delay(500 - (now - lastPublishTime));
    }
    lastPublishTime = millis();

    uint8_t version = data[offset[0]];
    batteryLevel = data[offset[1]];
    x = GetAccelerometer(data[offset[2]], data[offset[3]]);
    y = GetAccelerometer(data[offset[4]], data[offset[5]]);
    z = GetAccelerometer(data[offset[6]], data[offset[7]]);

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
            static DynamicJsonDocument doc(640);  // buffer maior para evitar overflow/{ }
            static char jsonBuf[640];             // static to stay off the heap
            doc.clear();

            doc["messageType"] = "accelerometer";
            doc["gatewayId"] = DEVICE_ID;
            doc["deviceId"] = macAddress;
            doc["mac"] = macAddress;
            doc["battery"] = batteryLevel;
            doc["vendor"] = vendor;
            doc["sourceUuid"] = (offset == minewOffSet) ? "0xFFE1" : (offset == mokoOffSet) ? "0xFEAB" : "unknown";
            
            // Usar chaves prefixadas por vendor para evitar mistura no ThingsBoard
            if (strcmp(vendor, "minew") == 0) {
                doc["x_minew"] = roundf(x * 1000) / 1000;
                doc["y_minew"] = roundf(y * 1000) / 1000;
                doc["z_minew"] = roundf(z * 1000) / 1000;
            } else if (strcmp(vendor, "moko") == 0) {
                doc["x_moko"] = roundf(x * 1000) / 1000;
                doc["y_moko"] = roundf(y * 1000) / 1000;
                doc["z_moko"] = roundf(z * 1000) / 1000;
            } else {
                doc["x"] = roundf(x * 1000) / 1000;
                doc["y"] = roundf(y * 1000) / 1000;
                doc["z"] = roundf(z * 1000) / 1000;
            }
            
            doc["rssi"] = rssi;
            doc["deviceType"] = deviceType;
            doc["ts"] = connect->getEpochMillis();
            
            size_t written = serializeJson(doc, jsonBuf, sizeof(jsonBuf));
            if (written > 0 && written < sizeof(jsonBuf)) {
                connect->loopMQTT();
                if (connect->publishTelemetryRaw(jsonBuf, written)) {
                    Serial.printf("✅ Acelerometro enviado: %s\n", jsonBuf);
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

    const int accelLimit = 60;
    int accelProcessed = 0;

    for (int i = 0; i < maxDevices; i++)
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
                    if (accelProcessed >= accelLimit)
                    {
                        continue;
                    }

                    if (len < 1)
                        continue;

                    if (data[0] != item.type)
                    {
                        continue;
                    }

                    size_t minRequired = (size_t)(*std::max_element(item.offset.begin(), item.offset.end()) + 1);
                    if (len < minRequired)
                    {
                        continue;
                    }

                    // Safe cast (processAccelerometer expects uint8_t*)
                    processAccelerometer((uint8_t *)data, len, item.offset);
                    accelProcessed++;

                }
                else
                {
                    // Telemetry (Eddystone TLM normally 0x20, with 14 bytes)
                    if (len >= 14)
                    {
                        processTelemetry((uint8_t *)data, len);
                    }
                }
            }
        }
        catch (...)
        {
            continue;
        }
    }
    if (accelProcessed >= accelLimit)
    {
        Serial.printf("⚠️  Accelerometer limit atingido (%d). Aguardando próximo ciclo.\n", accelLimit);
    }

    Serial.printf("✅ ListDevices() completou com sucesso!\n");
}
