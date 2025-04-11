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
#include "headers/User.h"
#include "headers/Distributor.h"
#include "headers/Advertisements.h"

//TODO: O Próximo passo é conseguir abstrair toda a parte de WI-FI para uma nova classe

WiFiServer server(80);
HTTPClient http;
Preferences preferences;

#define main_h
#define SCAN_INTERVAL 3000
int MAX_ERROR_MODE = 1800000;
#define TARGET_RSSI -90 //RSSI limite para ligar o relê.
#define MAX_MISSING_TIME 7000 //Tempo para desligar o relê desde o momento que o iTag não for encontrado

BLEScan* pBLEScan;
BLEScanResults foundDevices;
String placa = "FAB_SJC_CE1_T_0007";

String apiUrl = "http://brd-parque-it.pointservice.com.br/api/v1/IOT";

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
; //Quando ocorreu o último scan

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

Distributor* distributor = nullptr;

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

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
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
    Serial.printf("\n📡 POST para API - Dispositivo %s:\n", mac);
    Serial.printf("┌──────────────────────────────────\n");
    Serial.printf("│ 👤 Usuário: %s\n", userId);
    Serial.printf("│ 📊 Média RSSI: %d\n", media);
    Serial.printf("│ ⏰ Tempo: %s\n", tempo);
    Serial.printf("│ 📱 MAC: %s\n", mac);
    Serial.printf("│ 🔢 Tipo: %d\n", deviceType);
    Serial.printf("│ 🔋 Bateria: %d%%\n", batteryLevel);
    if (deviceType == 4) {  // Se for acelerômetro
        Serial.printf("│ 🎯 Acelerômetro:\n");
        Serial.printf("│   ➡️ X: %.2f\n", x);
        Serial.printf("│   ⬆️ Y: %.2f\n", y);
        Serial.printf("│   ↗️ Z: %.2f\n", z);
    }
    Serial.printf("│ ⏱️ Tempo Ativo: %.1f dias\n", timeActivity);
    Serial.printf("└──────────────────────────────────\n");
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
      Serial.printf("\n┌──────────────────────────────────\n");
      Serial.printf("│ 🌐 URL: %s/PostLocation\n", apiUrl.c_str());
      Serial.printf("└──────────────────────────────────\n");
      
      if (http.begin((apiUrl + "/PostLocation").c_str())) 
      {
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(15000);

        int httpCode = http.POST(json);
        Serial.printf("\n┌──────────────────────────────────\n");
        Serial.printf("│ 📡 Status: %d %s\n", httpCode, httpCode == 200 ? "✅ OK" : httpCode >= 500 ? "❌ Erro no Servidor" : httpCode >= 400 ? "⚠️ Erro na Requisição" : "⚡ Erro na Conexão");
        Serial.printf("└──────────────────────────────────\n");

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

void loadErrorMode() {
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
        preferences.begin("my-app", false);
        String s = preferences.getString("erros", "yyy");
        int erros = preferences.getInt("qtdErros", 0);
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected()) {            // loop while the client's connected
            timerWrite(timer, 0); //reset timer (feed watchdog)
            
            if (client.available()) {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                Serial.write(c);                    // print it out the serial monitor
                header += c;
                if (c == '\n') {                    // if the byte is a newline character
                    if (currentLine.length() == 0) {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");
                        client.println("<style>");
                        client.println("body {");
                        client.println("background-color: #f5ab00;");
                        client.println("}");
                        client.println("</style");

                        client.println("<body><h1> ESP32-LOG DE ERROS</h1>");
                        client.println("<hr>");
                        client.println("<p>" + s + "</p>");
                        client.println("<div style='text-align: center;'>");
                        client.println("<a href=\"/?buttonRestart\"\"><button style='font-size:150%; background-color:lightblue; color:red;border-radius:100px;'>RESET</button></a>");
                        client.println("</div>");

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
                        preferences.end();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        header = "";
        client.stop();
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
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(2000);
    pBLEScan->setWindow(1999); 
    

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    distributor = new Distributor(User::getAllUsers(), TIME_MEDIA, SCAN_INTERVAL, pBLEScan);
    
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
    timerWrite(timer, 0);

    if(validateStatusWIFI())
    {
      timeClient.forceUpdate();
      if (distributor != nullptr) {
        distributor->process();
      }
      getOn(sendId);
    }
  }
}