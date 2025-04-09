#include "esp_system.h"
#include <ResponsiveAnalogRead.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "BLEService.h"
#include <BLEBeacon.h>
#include <BLEEddystoneTLM.h>
#include <BLEEddystoneURL.h>
#include <HTTPClient.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <string.h>
#include <cstdlib>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <sstream>
#include <Preferences.h>
#include <HTTPUpdate.h>
#include <iostream>
#include <cmath>
#include <map>

WiFiServer server(80);
HTTPClient http;
Preferences preferences;

BLEScan* pBLEScan; //Variável que irá guardar o scan
BLEScanResults foundDevices;

#define SCAN_INTERVAL 3000
int MAX_ERROR_MODE = 1800000;
#define TARGET_RSSI -90 //RSSI limite para ligar o relê.
#define MAX_MISSING_TIME 7000 //Tempo para desligar o relê desde o momento que o iTag não for encontrado
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define ENDIAN_CHANGE_U32(x) ((((x)&0xFF000000)>>24) + (((x)&0x00FF0000)>>8)) + ((((x)&0xFF00)<<8) + (((x)&0xFF)<<24))
//Tempo que necessário para a identificação da posição


String placa = "FAB_SJC_CE1_T_0007";

String apiUrl = "http://brd-parque-it.pointservice.com.br/api/v1/IOT";
//String apiUrl = "https://localhost:44331/api/v1/IOT";

String versionId = "11.0.0.0v-point-pqtech-beta0015082024.bin";

const char* NOME =  "pointservice";
const char* SENHA = "HGgs@250";

const char* NOME_ALTERNATIVA = "FAB&ITO1";
const char* SENHA_ALTERNATIVA = "CidadeInteligente";

const char* server_password = "ErrorLogServer123";
String header;
bool errorMode = false;
bool sending = false;

String _id = "0";
String sendId;
bool anterior = false;
int tentativas = 0;
int media = 0;
int comunicationErrors = 0;
uint32_t lastScanTime = 0; //Quando ocorreu o último scan

uint32_t lastSendTime = 0;
uint32_t inicioMedia = 0;
uint32_t initErrorMode = 0;
std::string miId;
std::string mId;

struct readsvertor {
  String address;
  int value = 0;
};

std::vector<readsvertor> allreads;

struct user {
  String id;
  std::vector<int> mediasRssi;
  bool loggedIn;
  int batteryLevel = 0;
  float x = 0;
  float y = 0;
  float z = 0;
  int vezes = 0;
  String tempo;
  String mac;
  int deviceTypeUser;
  float timeActivity;
  ResponsiveAnalogRead analog = ResponsiveAnalogRead(0, true, 0.8);
};

std::vector<user> allUsers;

struct CompanyIdentifier {
    uint16_t code;
    const char* name;
};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000);
String formattedDate;
String initialId = "252120";
String finalId = "441a802f9c398fc199d2";
String manufacturerAndroid = "1d00";
String manufacturerIOS = "4c00";
int TIME_MEDIA = 100000;
int TIME_ACTIVE = 1200000; 
int WIFI_LIMIT = -80;

bool tagPressed = false;
bool findPressed = false;

long initTagPressed;
long EndTagPressed;
long initFindPressed;
long EndFindPressed;

const int wdtTimeout = 300000;  // time in ms to watchdog --> 5 minutos
hw_timer_t *timer = NULL;
int battery_pin = A3;

std::vector<std::string> split(std::string stringToBeSplitted, std::string delimeter)
{
  std::vector<std::string> splittedString;
  int startIndex = 0;
  int  endIndex = 0;
  while ( (endIndex = stringToBeSplitted.find(delimeter, startIndex)) < stringToBeSplitted.size() )
  {
    std::string val = stringToBeSplitted.substr(startIndex, endIndex - startIndex);
    splittedString.push_back(val);
    startIndex = endIndex + delimeter.size();
  }
  if (startIndex < stringToBeSplitted.size())
  {
    std::string val = stringToBeSplitted.substr(startIndex);
    splittedString.push_back(val);
  }
  return splittedString;
}

std::vector<std::string> separatestring(std::string str, int n)
{
  int size_of_str = str.size();
  int i;
  int size_of_part;
  std::vector<std::string> returnString;

  // Check for the string if it can be divided or not
  if (size_of_str % n != 0)
  {
    ////Serial.println("Invalid");
    return returnString;
  }

  // Get the size of parts to get the division
  size_of_part = size_of_str / n;
  std::string c;
  for (i = 0; i < size_of_str; i++)
  {
    c += str[i];
    ////Serial.println(c.c_str());
    if ((i + 1) % size_of_part == 0) {
      returnString.push_back(c);
      c.clear();
    }

  }
  return returnString;
}

int findUser(String id) {
  int i = 0;
  while (i < allUsers.size()) {
    if (allUsers[i].id == id) {
      return i;
    }
    i++;
  }
  return -1;
}

float battery_read(float batteryVoltage)
{
    float percentage = 0;

    if(batteryVoltage > 3.0){
      percentage = 100;
    }else if(batteryVoltage > 2.9 && batteryVoltage <= 3.0){
      percentage = 100 - (3.0 - batteryVoltage) * 58/0.1;
    }else if(batteryVoltage > 2.74 && batteryVoltage <= 2.9){
      percentage = 42 - (2.9 - batteryVoltage) * 24/0.16;
    }else if(batteryVoltage > 2.44 && batteryVoltage <= 2.74){
      percentage = 18 - (2.74 - batteryVoltage) * 12/0.3;
    }else if(batteryVoltage > 2.1 && batteryVoltage <= 2.44){
      percentage = 6 - (2.44 - batteryVoltage) * 6/0.34;
    }else if(batteryVoltage < 2.1){
      percentage = 0; 
    }
    return std::ceil(percentage);
}

float getAdvertisingDataAcelerometerV3(uint8_t msb, uint8_t lsb) {
    // Constrói o valor de 16 bits a partir dos dois bytes
    uint16_t rawData = (msb << 8) | lsb;

    // Extrai a parte inteira e fracionária
    int integerPart = (rawData >> 8) & 0xFF;
    int fractionalPart = rawData & 0xFF;

    // Converte a parte fracionária para o formato decimal
    float fractionalValue = fractionalPart / 256.0;

    // Soma a parte inteira com a parte fracionária
    float result = integerPart + fractionalValue;

    // Tratamento para valores negativos
    if (rawData & 0x8000) {
        result -= 256.0;
    }

    return result;
}

void activeSoftAP() {
  if (errorMode) {
    initErrorMode = millis();
    // Connect to Wi-Fi network with SSID and password
    // //Serial.print("Setting AP (Access Point)…");
    // Remove the password parameter, if you want the AP (Access Point) to be open

    preferences.begin("my-app", false);

    String s = preferences.getString("idPlaca");
    delay(50);
    if (s == "") {
      s = WiFi.macAddress();
    }
    preferences.end();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(s.c_str(), server_password);

    IPAddress IP = WiFi.softAPIP();
    server.begin();
  }
}

bool validateStatusWIFI()
{
  if (WiFi.RSSI() < WIFI_LIMIT) 
  {
    Serial.println("Status Wifi: Desconectado (Potência baixa)");
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    //----------------------------------------- Tratativa Acrescentada para Erro WiFi_Out_Range ----------------------------------
    String json;
    const size_t capacity = JSON_OBJECT_SIZE(8);
    DynamicJsonDocument doc(capacity);

    doc["rssiWifi"] = WiFi.RSSI();
    serializeJson(doc, json);

    preferences.begin("my-app", false);
    ////Serial.println("Enviando...");
    String s = preferences.getString("erros", "yyy");
    int erros = preferences.getInt("qtdErros", 0);
    if (s == "yyy") {
      preferences.putString("erros", "Erro;Dados<br>ErroWiFi_Out_Range;" + json + "<br>");
      preferences.putInt("qtdErros", erros + 1);
    }
    else {
      String saving = s + "ErroWiFi_Out_Range;" + json + "<br>";
      ////Serial.println(saving);
      preferences.putString("erros", saving);
      preferences.putInt("qtdErros", erros + 1);
    }
    if (erros > 10 && erros < 15) {
      errorMode = true;
      activeSoftAP();
    } else if (erros > 15) { // Tratativa para não estourar o buffer de erros, caso os erros OFFLINE's sejam lidos por varias vezes antes da placa de fato esperar o MAX_ERROR_MODE se completar para depois zerar... nisso, eu zero caso o erro ultrapassar de 15...
      preferences.begin("my-app", false);
      preferences.putInt("qtdErros", 0);
      preferences.putString("erros", "");
    }
    preferences.end();
    //----------------------------------------- Tratativa Acrescentada para Erro WiFi_Out_Range ----------------------------------
    WiFi.disconnect();
  }

  if (WiFi.status() != WL_CONNECTED) 
  {
    Serial.println("Status Wifi: Desconectado");
    WiFi.begin(NOME, SENHA);
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    while (WiFi.status() != WL_CONNECTED) 
    {
      Serial.print("Network: ");
      Serial.println(NOME);
      timerWrite(timer, 0); //reset timer (feed watchdog)

      //----------------------------------  Tratativa acrescentada para Iniciar de fato com a primeira tentativa de conexão com SSID e SENHA --------------------------------
      if (tentativas == 3) { // Na primeira tentativa de conexão, o recurso WiFi.begin está puxando o SSID como NULL. E não se conecta de fato com o SSID correto.
        Serial.print("Network: ");
        Serial.println(NOME);
        WiFi.begin(NOME, SENHA);
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
      }
      //----------------------------------  Tratativa acrescentada para Iniciar de fato com a primeira tentativa de conexão com SSID e SENHA --------------------------------

      if (tentativas <= 10) {
        delay(1000);
        Serial.print(tentativas);
        Serial.print(".");

        //---------  Tratativa acrescentada para após o fracasso em tentar se conectar com o primeiro SSID e SENHA, registrar esse erro em Preference --------------------------

        if (tentativas == 10) {
          String json;
          const size_t capacity = JSON_OBJECT_SIZE(8);
          DynamicJsonDocument doc(capacity);

          doc["wifiSsid"] = NOME;
          serializeJson(doc, json);
          ////Serial.println(json);

          preferences.begin("my-app", false);
          ////Serial.println("Enviando...");
          String s = preferences.getString("erros", "yyy");
          int erros = preferences.getInt("qtdErros", 0);
          if (s == "yyy") {
            preferences.putString("erros", "Erro;Dados<br>ErroWiFi_First_Connection;" + json + "<br>");
            preferences.putInt("qtdErros", erros + 1);
          }
          else {
            String saving = s + "ErroWiFi_First_Connection;" + json + "<br>";
            ////Serial.println(saving);
            preferences.putString("erros", saving);
            preferences.putInt("qtdErros", erros + 1);
          }
          if (erros > 10 && erros < 15) {
            errorMode = true;
            activeSoftAP();
            break;
          } else if (erros > 15) { // Tratativa para não estourar o buffer de erros, caso os erros OFFLINE's sejam lidos por varias vezes antes da placa de fato esperar o MAX_ERROR_MODE se completar para depois zerar... nisso, eu zero caso o erro ultrapassar de 15...
            preferences.begin("my-app", false);
            preferences.putInt("qtdErros", 0);
            preferences.putString("erros", "");
          }
          preferences.end();
        }
        //---------  Tratativa acrescentada para após o fracasso em tentar se conectar com o primeiro SSID e SENHA, registrar esse erro em Preference --------------------------
      }
      else 
      {
        if (tentativas == 11) 
        {
          Serial.print("Network: ");
          Serial.println(NOME_ALTERNATIVA);
          WiFi.begin(NOME_ALTERNATIVA, SENHA_ALTERNATIVA);
          Serial.print("RSSI: ");
          Serial.println(WiFi.RSSI());
        }
        
        if (tentativas == 20) 
        {
          Serial.print("Network: ");
          Serial.println(NOME);
          WiFi.begin(NOME, SENHA);
          Serial.print("RSSI: ");
          Serial.println(WiFi.RSSI());
          tentativas = 0;

          //---------  Tratativa acrescentada para após o fracasso em tentar se conectar com o segundo SSID e SENHA, registrar esse erro em Preference --------------------------
          String json;
          const size_t capacity = JSON_OBJECT_SIZE(8);
          DynamicJsonDocument doc(capacity);

          doc["wifiSsid"] = NOME_ALTERNATIVA;
          serializeJson(doc, json);
          ////Serial.println(json);

          preferences.begin("my-app", false);
          ////Serial.println("Enviando...");
          String s = preferences.getString("erros", "yyy");
          int erros = preferences.getInt("qtdErros", 0);
          if (s == "yyy") {
            preferences.putString("erros", "Erro;Dados<br>ErroWiFi_Second_Connection;" + json + "<br>");
            preferences.putInt("qtdErros", erros + 1);
          }
          else {
            String saving = s + "ErroWiFi_Second_Connection;" + json + "<br>";
            ////Serial.println(saving);
            preferences.putString("erros", saving);
            preferences.putInt("qtdErros", erros + 1);
          }
          if (erros > 10 && erros < 15) {
            errorMode = true;
            activeSoftAP();
            break;
          } else if (erros > 15) { // Tratativa para não estourar o buffer de erros, caso os erros OFFLINE's sejam lidos por varias vezes antes da placa de fato esperar o MAX_ERROR_MODE se completar para depois zerar... nisso, eu zero caso o erro ultrapassar de 15...
            preferences.begin("my-app", false);
            preferences.putInt("qtdErros", 0);
            preferences.putString("erros", "");
          }
          preferences.end();
          //---------  Tratativa acrescentada para após o fracasso em tentar se conectar com o segundo SSID e SENHA, registrar esse erro em Preference --------------------------

        }
        delay(1000);
        Serial.print(tentativas);
        Serial.print(".");
      }
      tentativas++;
    }
    tentativas = 0;
  }
  return tentativas == 0;
}

void registraDadosDoUsuario(String macAddress, String code, int rssiBLE, int deviceType, int batterylevel, float x, float y, float z, float timeActivity)
{
  if (allUsers.size() == 0) 
  {
    inicioMedia = millis();
    user firstUser;
    firstUser.id = code;
    firstUser.batteryLevel = batterylevel;
    firstUser.x = x;
    firstUser.y = y;
    firstUser.z = z;
    firstUser.analog = ResponsiveAnalogRead(0, true, 0.8);
    firstUser.analog.update((rssiBLE * -1));
    firstUser.mediasRssi.push_back(firstUser.analog.getValue());
    firstUser.tempo = formattedDate;
    firstUser.mac = macAddress;
    firstUser.timeActivity = timeActivity;
    firstUser.deviceTypeUser = deviceType;
    allUsers.push_back(firstUser);
  }
  else 
  {
    int foundUser = 0;
    foundUser = findUser(code);
    if (foundUser != -1) {
      if(allUsers[foundUser].batteryLevel == 0 && batterylevel > 0)
      {
        allUsers[foundUser].batteryLevel = batterylevel;
      }
      allUsers[foundUser].x = x;
      allUsers[foundUser].y = y;
      allUsers[foundUser].z = z;

      allUsers[foundUser].analog.update((rssiBLE * -1));
      allUsers[foundUser].mediasRssi.push_back(allUsers[foundUser].analog.getValue());
      allUsers[foundUser].tempo = formattedDate;
      allUsers[foundUser].timeActivity = timeActivity;
    }
    else 
    {
      user newUser;
      newUser.id = code;

      newUser.batteryLevel = batterylevel;
      newUser.x = x;
      newUser.y = y;
      newUser.z = z;

      newUser.analog = ResponsiveAnalogRead(0, true, 0.8);
      newUser.analog.update((rssiBLE * -1));
      newUser.mediasRssi.push_back(newUser.analog.getValue());
      newUser.tempo = formattedDate;
      newUser.mac = macAddress;
      newUser.deviceTypeUser = deviceType;
      newUser.timeActivity = timeActivity;
      allUsers.push_back(newUser);
    }
  }
}

double distance(double sinalPower)
{
  if (sinalPower == 0) 
    return 0.0;

  const double n = 2.8; 
  const double A = -60;
  double B = 0.0;

  switch ((int)sinalPower) {
    case -90 ... -86:
      B = 5.5;
      break;
    case -85 ... -83:
      B = 5.2;
      break;
    case -82 ... -81:
      B = 5.0;
      break;
    case -80 ... -78:
      B = 4.8;
      break;
    case -77 ... -76:
      B = 4.5;
      break;
    case -75 ... -73:
      B = 4.2;
      break;
    case -72 ... -71:
      B = 4.0;
      break;
    case -69 ... -67:
      B = 3.2;
      break;
    case -66 ... -65:
      B = 2.8;
      break;
    case -64:
      B = 2.5;
      break;
    case -63:
      B = 2.0;
      break;
    case -62:
      B = 1.5;
      break;
    case -61:
      B = 1.0;
      break;
    default:
      B = 0.0;
      break;
  }  
  double distance = pow(10, (A - sinalPower) / (10 * n)) + B;
  
  Serial.printf("distance: %f\n", distance);
  return distance;
}

void getIBeacon(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
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

void getTelemetry(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
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
          Serial.printf("Porcentagem da Bateria %f%%\n", battery_read((float)batteryVoltage / 1000));
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

void getAccelerometer(BLEAdvertisedDevice advertisedDevice, uint8_t* payload, size_t length)
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
          float x =  getAdvertisingDataAcelerometerV3(data[3], data[4]);
          float y =  getAdvertisingDataAcelerometerV3(data[5], data[6]);
          float z =  getAdvertisingDataAcelerometerV3(data[7], data[8]);
          
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

int getAdvertisingDataBatteryV3(uint8_t payload)
{
  char valorHex[3]; // Para armazenar o valor hexadecimal como string
  
  // Convertendo uint8_t para hexadecimal
  snprintf(valorHex, sizeof(valorHex), "%02X", payload);

  // Convertendo a string hexadecimal de volta para um inteiro
  int valorInteiro;
  sscanf(valorHex, "%X", &valorInteiro);
  
  return valorInteiro;
}

void ListDevices()
{
  BLEAdvertisedDevice advertisedDevice;
  String device;
  String macAddressDevice;
  uint8_t* payload = nullptr;
  size_t length = 0;
  std::string serviceData;

  foundDevices = pBLEScan->start(3);
  int deviceCount = foundDevices.getCount();
  
  Serial.printf("Founded Devices: %d\n", deviceCount);
  lastScanTime = millis(); 

  for (uint32_t i = 0; i < deviceCount; i++)
  {
    advertisedDevice = foundDevices.getDevice(i);
    device = advertisedDevice.toString().c_str();
    macAddressDevice = device.substring(device.indexOf("Address: ")+9,device.indexOf("Address: ")+26);
    macAddressDevice.toUpperCase();

    payload = advertisedDevice.getPayload();
    length = advertisedDevice.getPayloadLength();
    serviceData = advertisedDevice.getServiceData();

    getIBeacon(advertisedDevice, payload, length);
    getTelemetry(advertisedDevice, payload, length);
    getAccelerometer(advertisedDevice, payload, length);
  }
    
  pBLEScan->clearResults();
}

double calculateDistance(const std::vector<int>& numbers) {
    
    int more_repeat = 0;
    int repeat = 0;
    int counter = 0;

    for (int i = 0; i < numbers.size(); i++) {
      more_repeat = numbers[0];
      repeat = 0;
      counter = 0;
      if(numbers[i] == numbers[i+1]){
        repeat++;
      }else{
        repeat = 1;
      }

      if(repeat > counter){
        counter = repeat;
        more_repeat = numbers[i];
      }
    }

    Serial.printf("Moda: %d\n", more_repeat);

    return more_repeat;
}

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void loggedIn(int pos) {
  if (allUsers[pos].mediasRssi.size() >= 3) {
    allUsers[pos].loggedIn = true;
    Serial.print("Device loggedIn: ");
    Serial.println(allUsers[pos].mediasRssi.size());
  }
  else {
    Serial.println("Device NOT loggedIn: Minimum (3)");
    allUsers[pos].loggedIn = false;
    allUsers[pos].vezes = 0;
  }
}

void updatePreferences(){
  preferences.begin("my-app", false);
  String s = preferences.getString("idPlaca");
  delay(500);

  if (s == "") {
    sendId =  placa;
    preferences.putString("idPlaca", placa);
  }
  else if (placa != "") 
  {
    if (placa == s) {
      sendId = s;
    }  
    else 
    {
      Serial.println("Atualizando nome de placa... (RESET)");
      preferences.putString("idPlaca", placa);
      sendId = placa;
    }
  }
  else 
  {
    sendId = s;
  }

  preferences.end();
}

void postIn(String userId, int media, String tempo, String mac, int deviceType, int batteryLevel, float x, float y, float z, float timeActivity) {
  if (validateStatusWIFI()) 
  {
    String json;
    const size_t capacity = JSON_OBJECT_SIZE(25);
    DynamicJsonDocument doc(capacity);

    const char* timeNow = tempo.c_str();

    timeClient.forceUpdate(); // Atualização do tempo para poder enviar o "timing" correto
    String sendTime = timeClient.getFormattedTime();
    const char* timeMicrocontroller = sendTime.c_str();
    
    if(userId.startsWith("Itag_") && tagPressed == true){
       doc["iTagPressed"] = "true";
       tagPressed = false;
    }
    
       doc["iTagPressed"] = "true";
       findPressed = false;
    
    
    /*if(deviceType == 2)
    {
       doc["userCode"] = userId.toInt();
    } else {
       doc["deviceCode"] = userId;
    }*/

    doc["userCode"] = userId.toInt();
    doc["microcontrollerId"] = sendId;
    doc["typeEvent"] = 1;
    doc["sinalPower"] = media;
    doc["timeInOut"] = timeMicrocontroller;
    doc["timeInOutUser"] = timeNow;
    doc["version"] = "1";
    //doc["deviceType"] = deviceType; // deviceType 1 = itag | 2 = smartphone | 3 = watchband | 4 = card
    doc["deviceCode"] = "Card_" + userId;
    doc["macAddress"] = mac;
    doc["x"] = x;
    doc["y"] = y;
    doc["z"] = z;
    doc["batteryLevel"] = batteryLevel;
    doc["timeActivity"] = timeActivity;

    serializeJson(doc, json);
    Serial.println(json);
    Serial.println(apiUrl+"/PostLocation");
    
    if (http.begin((apiUrl + "/PostLocation").c_str())) 
    {
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(15000);

      int httpCode = http.POST(json);
      Serial.print("HTTPCODE: ");
      Serial.println(httpCode);

      String result = http.getString();
      DynamicJsonDocument payload(4776);
      deserializeJson(payload, result);
      Serial.println(result);
      _id = payload["_id"].as<String>();
      http.end();
    } else {
      Serial.println("Erro na conexão");
    }
  }
}

void updatePlaca(String url)
{
  ////Serial.println("Board updating...");
  WiFiClient client;
  t_httpUpdate_return ret = httpUpdate.update(client, url, "1.0.0.0");
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      ////Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      ////Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      ////Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void getOn(String s) {

  if (millis() - lastSendTime > TIME_ACTIVE) 
  {
    Serial.println();
    Serial.print("Enviado TURNON: ");
    Serial.println(s);
    
    lastSendTime = millis(); // Zera o millis() dentro de lastSendTime para não zerar a contagem do TIME_ACTIVE

    if(validateStatusWIFI())
    {
      StaticJsonDocument<1500> document;
      int wifiRssi = WiFi.RSSI() * -1;
    
      StaticJsonDocument<200> doc;
      String output;
      doc["code"] = s;
      doc["rssi"] = wifiRssi;
      doc["version"] = versionId;
     
      serializeJson(doc, output);
      
      if (http.begin((apiUrl + "/TurnOn").c_str())) {
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(15000);
        
        int httpCode = http.POST(output);
        if (httpCode >= 200) {
          if (httpCode == 200) {
            lastSendTime = millis();
            String result = http.getString();
            //StaticJsonDocument<192> doc;
            DeserializationError error = deserializeJson(document, result);

            if (error) {
              ////Serial.print(F("deserializeJson() failed: "));
              return;
            }
            else {
              String montagemString = "";
              int dataVerify = 0;
              
              const char* initialString = document["initial"];
              String stringVerify = initialString;
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                initialId = initialString;
                montagemString = "initialId: " + initialId;
              } else {
                montagemString = "initialId com preenchimento zerado!!! Ignorando dado recebido...";
              }

              const char* finalString = document["final"];
              stringVerify = finalString;
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
              finalId = finalString;
              montagemString = " finalId: " + finalId;
              } else {
                montagemString = "finalId com preenchimento zerado!!! Ignorando dado recebido...";
              }

              const char* manufacturerString = document["manufacturer"];
              stringVerify = manufacturerString;
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                manufacturerAndroid = manufacturerString;
                montagemString = " manufacturer: " + manufacturerAndroid; 
              } else {
                montagemString = "manufacturer com preenchimento zerado!!! Ignorando dado recebido...";
              }

              dataVerify = document["measureInterval"];
              stringVerify = String(dataVerify);
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                TIME_MEDIA = document["measureInterval"];
                montagemString = " TIME_MEDIA: " + String(TIME_MEDIA);
              } else {
                montagemString = "TIME_MEDIA com preenchimento zerado!!! Ignorando dado recebido...";
              }
              ////Serial.println(montagemString);

              dataVerify = document["limitSignalWiFi"];
              stringVerify = String(dataVerify);
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                WIFI_LIMIT = document["limitSignalWiFi"];
                WIFI_LIMIT = WIFI_LIMIT * -1;
                montagemString = " WIFI_LIMIT: " + String(WIFI_LIMIT);
              } else {
                montagemString = "WIFI_LIMIT com preenchimento zerado!!! Ignorando dado recebido...";
              }
              ////Serial.println(montagemString);

              if (document["maxErrorMode"]) {
                int t = document["maxErrorMode"];
                MAX_ERROR_MODE = t;
                stringVerify = String(t);
                if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                  montagemString = " MAX_ERROR_MODE: " + String(t);
                  preferences.begin("my-app", false);
                  preferences.putInt("maxErrorMode", t); // --> Salva o novo número de tempo no máxErrorMode na memória não volátil
                  preferences.end();
                } else {
                  montagemString = "MAX_ERROR_MODE com preenchimento zerado!!! Ignorando dado recebido...";
                }              
                ////Serial.println(montagemString);
              }


              stringVerify = document["controllerName"].as<String>();
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                sendId = document["controllerName"].as<String>();
                montagemString = " PlacaId: " + sendId;
              } else {
                montagemString = "PlacaId com preenchimento zerado!!! Ignorando dado recebido...";
              }

              stringVerify = document["apiURL"].as<String>();   
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){
                apiUrl = document["apiURL"].as<String>();
                montagemString = " apiURL: " + apiUrl;
              } else {
                montagemString = "apiURL com preenchimento zerado!!! Ignorando dado recebido...";
              }

              stringVerify = document["mode"].as<String>();   
              if(stringVerify.length() != 0 && stringVerify.length() != 1 && stringVerify != "null"){

                if(stringVerify == "Reset"){
                /*//Serial.println();
                //Serial.println("..............................................");
                //Serial.println("RECEBIDO COMANDO DE RESET VIA API");
                //Serial.println("..............................................");*/
                ESP.restart();
                } 
                else 
                {
                  /*//Serial.println("COMANDO DESCONHECIDO; Ignorando informação recebida!");
                  //Serial.println("-------------- Dado Recebido --------------------");
                  //Serial.println(stringVerify);
                  //Serial.println("-------------------------------------------------");*/
                }
              } else {
                montagemString = "mode com preenchimento zerado!!! Ignorando dado recebido...";
              }
            }
          }
          else {
            ////Serial.println("Falha nos dados enviados");
            ////Serial.println(httpCode);
          }

          ////Serial.println(httpCode);
          const char* messageReturnAPIchar = document["message"];              
          String messageReturnAPI = messageReturnAPIchar;
          ////Serial.println(messageReturnAPI);

          if (document["updateURL"]) {
            http.end();
            updatePlaca(document["updateURL"]);
          }
          else {
            http.end();
          }
        }
        else {
          comunicationErrors = comunicationErrors + 1;
          /*//Serial.print("Erro de comunicação: ");
          //Serial.println(comunicationErrors);
          //Serial.println("ERRO AO ESTABELECER CONEXÃO");
          //Serial.print("HTTP ERROR");
          //Serial.println(httpCode);*/

          String result = http.getString();
          DynamicJsonDocument payload(4776);
          deserializeJson(payload, result);
        }
      }
    }  
  }
}

void postOut(String userId, int deviceTypeOut) {
  if(validateStatusWIFI())
  {
    String json;
    const size_t capacity = JSON_OBJECT_SIZE(25);
    DynamicJsonDocument doc(capacity);

    timeClient.forceUpdate(); // Atualização do tempo para envio do "timing" correto
    String sendTime = timeClient.getFormattedTime();
    const char* timeMicrocontroller = sendTime.c_str();

    if(deviceTypeOut == 2){
       doc["userCode"] = userId.toInt();
    } else {
       doc["deviceCode"] = userId;
    }

    doc["microcontrollerId"] = sendId;
    doc["typeEvent"] = 2;
    doc["sinalPower"] = 0;
    doc["deviceType"] = deviceTypeOut; // deviceType 1 = itag | 2 = smartphone | 3 = watchband | 4 = card
    doc["timeInOut"] = timeMicrocontroller;
    serializeJson(doc, json);
    ////Serial.println(json);
    if (http.begin((apiUrl + "/PostLocation").c_str())) {
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(15000);
      
      int httpCode = http.POST(json);

      ////Serial.println(httpCode);
        
      String result = http.getString();
      DynamicJsonDocument payload(4776);
      deserializeJson(payload, result);
      ////Serial.println(result);
      ////Serial.println(payload["_id"].as<String>());
      _id = payload["_id"].as<String>();
      ////Serial.println("RESPOSTA");
      http.end();
    }
    else 
    {
      ////Serial.println("Erro na conexão");
    }
  }
}
  
String initStringOrInt(String messageToVerify){
  if(messageToVerify.startsWith("0")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("1")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("2")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("3")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("4")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("5")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("6")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("7")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("8")){
    return messageToVerify;
  } else if(messageToVerify.startsWith("9")){
    return messageToVerify;
  } else {
    return messageToVerify.substring(1);
  }
  
}

bool isZero(String messageToVerify){
  if(messageToVerify.startsWith("0")){
    return true;
  } else {
    return false;
  }
}

void LoopingsDeDados()
{
  if (millis() - inicioMedia > TIME_MEDIA) 
  {
    sending = true;
    Serial.println();
    Serial.println("Iniciando processo de envio");
    int i = 0;

    Serial.println();
    Serial.print("allUsers.size: ");
    Serial.println(allUsers.size());
    Serial.println();

    while (i < allUsers.size()) 
    {
      Serial.println();
      Serial.println("-----------------------User postIn---------------------------");
      Serial.print(F("Device: "));
      Serial.println(allUsers[i].id);
      Serial.print("Sending...(");
      Serial.print(i+1);
      Serial.println(")");

      loggedIn(i);
      if (allUsers[i].loggedIn)
      {
          int mode = calculateDistance(allUsers[i].mediasRssi);
          distance(mode*-1);
          allUsers[i].mediasRssi.clear();
          allUsers[i].vezes++;
          
          postIn(allUsers[i].id, mode, allUsers[i].tempo, allUsers[i].mac, allUsers[i].deviceTypeUser, allUsers[i].batteryLevel, allUsers[i].x, allUsers[i].y, allUsers[i].z, allUsers[i].timeActivity);
          delay(100);
      }
      i++;
    }

    inicioMedia = millis();
    sending = false;
    Serial.println();
    Serial.println("Encerrando processo de envio");
  }
  else if (millis() - lastScanTime > SCAN_INTERVAL && BLEDevice::getInitialized() == true && sending == false) 
  { 
    
    Serial.println(); 
    Serial.print("TIME to ACTIVE: ");
    Serial.println(millis() - lastSendTime);

    Serial.println(); 
    Serial.print("SCAN BLE");
    Serial.print(".");
    Serial.print(".");
    Serial.println("."); 
    ListDevices();
  }
  else
  {
    if(sending == true)
    {
      Serial.println("Em processo de envio: sending == true");
    }
    else
    {
      Serial.print(".");
    }
  }
}

void loadErrorMode(){
  timerWrite(timer, 0); //reset timer (feed watchdog)
  if (millis() - initErrorMode > MAX_ERROR_MODE) {
    preferences.begin("my-app", false);
    preferences.putInt("qtdErros", 0);
    preferences.putString("erros", "");
    preferences.end();
    ESP.restart();
  }

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    ////Serial.println("New Client.");
    preferences.begin("my-app", false);
    ////Serial.println("Enviando...");
    String s = preferences.getString("erros", "yyy");
    int erros = preferences.getInt("qtdErros", 0);// print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      timerWrite(timer, 0); //reset timer (feed watchdog)
      
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>");
            client.println("body {");
            client.println("background-color: #f5ab00;");
            client.println("}");
            client.println("</style");


            // Web Page Heading
            client.println("<body><h1> ESP32-LOG DE ERROS</h1>");
            client.println("<hr>");
            client.println("<p>" + s + "</p>");
            client.println("<div style='text-align: center;'>");
            client.println("<a href=\"/?buttonRestart\"\"><button style='font-size:150%; background-color:lightblue; color:red;border-radius:100px;'>RESET</button></a>");
            client.println("</div>");
            client.println("");
            client.println("");
            client.println("");
            client.println("");
            client.println("");
            client.println("");
            client.println("");

            //controls the Arduino if you press the buttons
            if (header.indexOf("?buttonRestart") > 0) {
              preferences.begin("my-app", false);
              preferences.putInt("qtdErros", 0);
              preferences.putString("erros", "");
              preferences.end();
              client.println("<h1 style='text-align: center;'>PLACA REINICIADA</h1>");
              delay(5000);
              ESP.restart();
            }


            client.println("</body></html>");
            client.stop();
            delay(1);

            // The HTTP response ends with another blank line
            //client.println();
            preferences.end();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    client.stop();
    ////Serial.println("Client disconnected.");
    ////Serial.println("");
  }
}

void setup() {

  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                          //enable interrupt
  
  Serial.begin(115200);

  WiFi.begin(NOME, SENHA);

  updatePreferences();

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // probalbly not needed
  WiFi.setHostname(sendId.c_str());

  if(validateStatusWIFI())
  {
    lastSendTime = (millis() - (TIME_ACTIVE + 1));
    getOn(sendId);

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN , ESP_PWR_LVL_P9);

    BLEDevice::init(""); 
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);  //active scan uses more power, but get results faster
    pBLEScan->setInterval(2000);
    pBLEScan->setWindow(1999);  // less or equal setInterval value

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
  }
}

void loop() {
  if (errorMode) 
  {
    loadErrorMode();
    //Serial.println("Dispositivo em modo de erro");
  }
  else 
  {
    timerWrite(timer, 0); //reset timer (feed watchdog)

    if(validateStatusWIFI())
    {
      timeClient.forceUpdate();
      LoopingsDeDados();
      getOn(sendId);
    }
  }
}