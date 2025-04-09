#include <sstream>
#include <HTTPUpdate.h>
#include <cmath>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>


#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF000000)>>24) + (((x)&0x00FF0000)>>8) + ((((x)&0xFF00)<<8) + (((x)&0xFF)<<24)))

extern void registraDadosDoUsuario(String macAddress, String code, int rssiBLE, int deviceType, int batterylevel, float x, float y, float z, float timeActivity);

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

void GetIBeacon(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
{
  if(advertisedDevice.haveManufacturerData()){
    std::string manufacturerData = advertisedDevice.getManufacturerData();
    uint8_t cManufacturerData[100];
    manufacturerData.copy((char*) cManufacturerData, manufacturerData.length(), 0);
    if(manufacturerData.length() == 25){
      Serial.printf("Raw Data: ");
      for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", payload[i]);   
      }
      
      Serial.println();
      Serial.printf("Device Found: %s \n", advertisedDevice.toString().c_str());
      Serial.printf("RSSI: %d \n", advertisedDevice.getRSSI());
      
      Serial.printf("Comprimento: %d\n", manufacturerData.length());
      Serial.printf("\n===============================\n");
      BLEBeacon oBeacon = BLEBeacon();
      oBeacon.setData(manufacturerData);
      Serial.printf("iBeacon Frame\n");
      Serial.printf("\n===============================\n");
      Serial.printf("Beacon Information\n");
      Serial.printf("ID: %04X\n", oBeacon.getManufacturerId());
      Serial.printf("Major: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMajor()));
      Serial.printf("Minor: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMinor()));
      Serial.printf("UUID: %s\n", oBeacon.getProximityUUID().toString().c_str());
      Serial.printf("Power: %d dBm\n", oBeacon.getSignalPower());
      Serial.printf("===============================\n\n");
    }   
  }
}

void GetTelemetry(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
{
  if(advertisedDevice.haveServiceData()){
    std::string serviceData = advertisedDevice.getServiceData();
    uint8_t data [100];
    serviceData.copy((char*)data, serviceData.length(), 0);
    if (advertisedDevice.getServiceDataUUID().equals(BLEUUID((uint16_t)0xFEAA))) {
      Serial.printf("Raw Data: ");
      for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", payload[i]);   
      }
      
      Serial.println();
      Serial.printf("Device Found: %s \n", advertisedDevice.toString().c_str());
      Serial.printf("RSSI: %d \n", advertisedDevice.getRSSI());
      Serial.printf("\n===============================\n");
      Serial.printf("Service Data: ");
      for (size_t i = 0; i < serviceData.length(); i++) {
          Serial.printf("%02X ", (uint8_t)serviceData[i]);
      }
      Serial.printf("===============================\n");
      Serial.printf("Eddystone Beacon Detectado\n");
      Serial.printf("UUID: %s\n", advertisedDevice.getServiceDataUUID().toString().c_str());
      Serial.printf("Comprimento: %d\n", serviceData.length());
      if (data[0] == 0x10) {
          Serial.printf("Eddystone Frame Type: Eddystone-URL\n");
          Serial.printf("===============================\n");
      } else if (data[0] == 0x20) {
        uint8_t version = data[1];
          uint16_t batteryVoltage = (data[2] << 8) | data[3];
          int16_t temperature = (data[4] << 8) | data[5];
          uint32_t packetCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
          uint32_t uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];
          Serial.printf("Eddystone Frame Type: Eddystone-TLM\n");
          Serial.printf("===============================\n");
          Serial.printf("Versão: %d\n", version);
          Serial.printf("Voltagem da Bateria: %d V\n", batteryVoltage);
          Serial.printf("Porcentagem da Bateria %f%%\n", GetBattery((float)batteryVoltage / 1000));
          Serial.printf("Temperatura: %.2f °C\n", temperature / 256.0);
          Serial.printf("Contador de Pacotes: %u\n", packetCount);
          Serial.printf("Tempo Ativo: %.1f days\n", (uptime / 10.0) / 86400);
          Serial.printf("===============================\n");
      } else if (data[0] == 0x00) {
          Serial.printf("Eddystone Frame Type: Eddystone-UID\n");
          Serial.printf("===============================\n");
      }    
    }
  }  
}

void GetAccelerometer(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
{
  String device = advertisedDevice.toString().c_str();

  String macTemp = device.substring(device.indexOf("Address: ")+21, device.indexOf("Address: ")+26);
  macTemp = macTemp.substring(0,2) + macTemp.substring(3,5);
  macTemp.toUpperCase();

  String macAddressDevice = device.substring(device.indexOf("Address: ")+9,device.indexOf("Address: ")+26);
  macAddressDevice.toUpperCase();

  float batterylevel = 0;
  float timeActivity = 0;

  if (advertisedDevice.haveServiceData()) {
      std::string serviceData = advertisedDevice.getServiceData();
      uint8_t data [100];
      serviceData.copy((char*)data, serviceData.length(), 0);
      if (data[0] == 0xA1 && advertisedDevice.getServiceDataUUID().equals(BLEUUID((uint16_t)0xFFE1))){ //Dispositivo Minew
          Serial.printf("Raw Data: ");
          for (size_t i = 0; i < length; i++) {
            Serial.printf("%02X ", payload[i]);   
          }
          Serial.printf("\n===============================\n");
          Serial.printf("Device Found: %s \n", advertisedDevice.toString().c_str());
          Serial.printf("RSSI: %d \n", advertisedDevice.getRSSI());
          Serial.printf("===============================\n");
          Serial.printf("Service Data: ");
          for (size_t i = 0; i < serviceData.length(); i++) {
              Serial.printf("%02X ", (uint8_t)serviceData[i]);
          }
          uint8_t version = data[1];
          uint16_t batteryLevel = data[2];
          float x =  GetAccelerometer(data[3], data[4]);
          float y =  GetAccelerometer(data[5], data[6]);
          float z =  GetAccelerometer(data[7], data[8]);
          
          Serial.printf("\nDispositivo Minew Encontrado\n");
          Serial.printf("Frame Type: Minew\n");
          Serial.printf("===============================\n");
          Serial.printf("Versão: %d\n", version);
          Serial.printf("Porcentagem da Bateria %d%%\n", batteryLevel);
          Serial.printf("X: %f\n", x);
          Serial.printf("Y: %f\n", y);
          Serial.printf("Z: %f\n", z);
          
          registraDadosDoUsuario(macAddressDevice, macTemp, advertisedDevice.getRSSI(), 4, batterylevel, x, y, z, timeActivity);
          Serial.printf("==========================================================================================================================================\n\n");
      }
      
    }
  
}