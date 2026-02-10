#include <BLEDevice.h>
#include "Distributor.h"
#include "Advertisements.h"
#include "Connect.h"
#include "config/Config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

extern Connect* connect;

const int SCAN_INTERVAL = 3000;
const int TIME_MEDIA = 30000;  // 30 segundos - garante tempo para acumular 3+ leituras (scans a cada 3s)

Distributor::Distributor(std::vector<User>& users, BLEScan* pBLEScan, String api_url) 
    : users(users),
      sending(false),
      inicioMedia(0),
      lastScanTime(0),
      lastSendTime(0),
      pBLEScan(pBLEScan),
      advertisements(new Advertisements()),
      API_URL(api_url)
{
}

double CalculateDistance(double signalPower)
{
  if (signalPower == 0) 
    return 0.0;

  const double n = 3.2;   // Propagação do sinal (quanto maior, mais distância)
  const double A = -55;   // RSSI de referência a 1 metro (menor valor = mais distância)
  double B = 0.0;

  // Ajustes de calibração AUMENTADOS para dar mais distância
  switch ((int)signalPower) {
    case -90 ... -86:
      B = 8.0;  // Aumentado
      break;
    case -85 ... -83:
      B = 7.5;  // Aumentado
      break;
    case -82 ... -81:
      B = 7.0;  // Aumentado
      break;
    case -80 ... -78:
      B = 6.5;  // Aumentado
      break;
    case -77 ... -76:
      B = 6.0;  // Aumentado
      break;
    case -75 ... -73:
      B = 5.5;  // Aumentado
      break;
    case -72 ... -71:
      B = 5.0;  // Aumentado
      break;
    case -69 ... -67:
      B = 4.5;  // Aumentado
      break;
    case -66 ... -65:
      B = 4.0;  // Aumentado
      break;
    case -64:
      B = 3.5;  // Aumentado
      break;
    case -63:
      B = 3.0;  // Aumentado
      break;
    case -62:
      B = 2.5;  // Aumentado
      break;
    case -61:
      B = 2.0;  // Aumentado
      break;
    default:
      B = 0.0;
      break;
  }  
  double distance = pow(10, (A - signalPower) / (10 * n)) + B;

  return distance;
}

int CalculateMode(const std::vector<int>& numbers) {
    int mostRepeated = 0;
    int repeat = 0;
    int counter = 0;

    for (int i = 0; i < numbers.size(); i++) {
      mostRepeated = numbers[0];
      repeat = 0;
      counter = 0;
      if(numbers[i] == numbers[i+1]){
        repeat++;
      }else{
        repeat = 1;
      }

      if(repeat > counter){
        counter = repeat;
        mostRepeated = numbers[i];
      }
    }

    return mostRepeated;
}

// ============================================================
// FUNÇÕES DE GEOLOCALIZAÇÃO
// ============================================================

/**
 * Estima um ângulo (em graus) baseado no RSSI para simular direção do beacon
 * Em uma implementação real, você poderia usar múltiplos gateways ou antenas 
 * para triangulação. Aqui, usamos o RSSI como base para variação angular.
 */
double Distributor::estimateAngleFromRSSI(int rssi) {
    // Usa o RSSI para criar variação no ângulo (0-360 graus)
    // Quanto mais forte o sinal (RSSI mais próximo de 0), mais próximo de norte (0°)
    // Sinais fracos distribuem em outras direções
    
    // Normalizar RSSI (-90 a -40) para (0 a 360 graus)
    double normalized = ((rssi + 90.0) / 50.0) * 360.0;
    
    // Garantir que está entre 0-360
    if (normalized < 0) normalized = 0;
    if (normalized > 360) normalized = 360;
    
    return normalized;
}

/**
 * Calcula latitude e longitude estimada do beacon baseado na distância e RSSI
 * Usa a localização fixa do gateway como referência
 * 
 * @param distance Distância em metros calculada a partir do RSSI
 * @param rssi Valor do RSSI em dBm
 * @param latitude (OUT) Latitude calculada do beacon
 * @param longitude (OUT) Longitude calculada do beacon
 */
void Distributor::calculateBeaconLocation(double distance, int rssi, double& latitude, double& longitude) {
    // Localização fixa do gateway (definida em Config.h)
    double gatewayLat = GATEWAY_LATITUDE;
    double gatewayLon = GATEWAY_LONGITUDE;
    
    // Estimar ângulo baseado no RSSI (em graus)
    double angle = estimateAngleFromRSSI(rssi);
    
    // Converter ângulo para radianos
    double angleRad = angle * (M_PI / 180.0);
    
    // Raio da Terra em metros
    const double EARTH_RADIUS = 6371000.0;
    
    // Calcular deslocamento em latitude e longitude
    // Fórmula simplificada para distâncias curtas (< 100m)
    double deltaLat = (distance * cos(angleRad)) / EARTH_RADIUS * (180.0 / M_PI);
    double deltaLon = (distance * sin(angleRad)) / (EARTH_RADIUS * cos(gatewayLat * M_PI / 180.0)) * (180.0 / M_PI);
    
    // Calcular posição final do beacon
    latitude = gatewayLat + deltaLat;
    longitude = gatewayLon + deltaLon;
    
    Serial.printf("📍 Geolocalização - Gateway: (%.6f, %.6f) | Distância: %.2fm | Ângulo: %.1f° | Beacon: (%.6f, %.6f)\n",
                  gatewayLat, gatewayLon, distance, angle, latitude, longitude);
}

void Distributor::loggedIn(int pos) {
  if (users[pos].getMediasRssi().size() >= 3) {
    users[pos].setLoggedIn(true);
    Serial.print("Device loggedIn: ");
    Serial.println(users[pos].getMediasRssi().size());
  }
  else {
    Serial.println("Device NOT loggedIn: Minimum (3)");
    users[pos].setLoggedIn(false);
    users[pos].setVezes(0);
  }
}

int Distributor::findUser(const String& id) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].getId() == id) {
            return i;
        }
    }
    return -1;
}

// Validar se o deviceCode corresponde aos últimos 4 caracteres do MAC address
bool Distributor::validateDeviceCodeWithMAC(const String& deviceCode, const String& macAddress) {
    if (deviceCode.length() < 4 || macAddress.length() < 2) {
        return false;
    }
    
    // Extrair últimos 4 caracteres do MAC (sem os ":")
    // Exemplo: macAddress = "84:CC:A8:2C:72:30" -> extrair "72:30" -> "7230"
    String macLast4 = macAddress.substring(macAddress.length() - 5);
    String macClean = "";
    for (int i = 0; i < macLast4.length(); i++) {
        if (macLast4[i] != ':') {
            macClean += macLast4[i];
        }
    }
    macClean.toUpperCase();
    
    // Extrair últimos 4 caracteres do código (ex: "Card_595C" -> "595C")
    String codeClean = deviceCode.substring(deviceCode.length() - 4);
    codeClean.toUpperCase();
    
    Serial.printf("🔍 Validando - Code: %s | MAC (últimos 4): %s\n", codeClean.c_str(), macClean.c_str());
    
    if (macClean == codeClean) {
        Serial.println("✓ DeviceCode VÁLIDO - Corresponde ao MAC!");
        return true;
    } else {
        Serial.printf("✗ AVISO: DeviceCode NÃO corresponde ao MAC! Pulando envio...\n");
        return false;
    }
}

void Distributor::UserRegisterData(const std::string& macAddress, const std::string& code, int rssiBLE, 
                                   int deviceType, int batterylevel, float x, float y, float z, 
                                   float timeActivity, String name) {
    if (users.empty()) {
        User firstUser;
        firstUser.setId(code.c_str());
        firstUser.setBatteryLevel(batterylevel);
        firstUser.setX(x);
        firstUser.setY(y);
        firstUser.setZ(z);
        firstUser.updateAnalog((rssiBLE * -1));
        firstUser.addMediaRssi(firstUser.getAnalog().getValue());
        firstUser.setMac(macAddress.c_str());
        firstUser.setTimeActivity(timeActivity);
        firstUser.setDeviceTypeUser(deviceType);
        users.push_back(firstUser);
    } else {
        int foundUser = findUser(code.c_str());
        if (foundUser != -1) {
            if(users[foundUser].getBatteryLevel() == 0 && batterylevel > 0) {
                users[foundUser].setBatteryLevel(batterylevel);
            }
            users[foundUser].setX(x);
            users[foundUser].setY(y);
            users[foundUser].setZ(z);
            users[foundUser].updateAnalog((rssiBLE * -1));
            users[foundUser].addMediaRssi(users[foundUser].getAnalog().getValue());
            users[foundUser].setTimeActivity(timeActivity);
        } else {
            User newUser;
            newUser.setId(code.c_str());
            newUser.setBatteryLevel(batterylevel);
            newUser.setX(x);
            newUser.setY(y);
            newUser.setZ(z);
            newUser.updateAnalog((rssiBLE * -1));
            newUser.addMediaRssi(newUser.getAnalog().getValue());
            newUser.setMac(macAddress.c_str());
            newUser.setDeviceTypeUser(deviceType);
            newUser.setTimeActivity(timeActivity);
            users.push_back(newUser);
        }
    }
}

void Distributor::postIn(String userId, int media, String tempo, String mac, 
                    int deviceType, int batteryLevel, float x, float y, float z, 
                    float timeActivity, String name, int mode, double distance) {
    Serial.printf("\n📡 POST to API - Device %s:\n", mac);
    Serial.printf("┌──────────────────────────────────\n");
    Serial.printf("│ 👤 User: %s\n", userId);
    Serial.printf("│ 📦 Name: %s\n", name);
    Serial.printf("│ 📊 RSSI Average: %d\n", media);
    Serial.printf("│ ⏰ Time: %s\n", tempo);
    Serial.printf("│ 📱 MAC: %s\n", mac);
    Serial.printf("│ 🔢 Type: %d\n", deviceType);
    Serial.printf("│ 🔋 Battery: %d%%\n", batteryLevel);
    Serial.printf("│ ⏱️ Active Time: %.1f days\n", timeActivity);
    Serial.printf("│ 📏 Distance: %.2f m\n", distance);
    Serial.printf("│ 📡 Mode: %d\n", mode);
    if (deviceType == 4) {
        Serial.printf("│ 🎯 Accelerometer:\n");
        Serial.printf("│   ➡️ X: %.2f\n", x);
        Serial.printf("│   ⬆️ Y: %.2f\n", y);
        Serial.printf("│   ↗️ Z: %.2f\n", z);
    }
    Serial.printf("└──────────────────────────────────\n");

    if (connect->validateStatusWIFI()) {
        String json;
        const size_t capacity = JSON_OBJECT_SIZE(25);
        DynamicJsonDocument doc(capacity);

        const char* timeNow = tempo.c_str();
        String sendTime = connect->getTime();
        const char* timeMicrocontroller = sendTime.c_str();

        doc["userCode"] = userId.toInt();
        doc["microcontrollerId"] = connect->getSendId();
        doc["typeEvent"] = 1;
        doc["sinalPower"] = media;
        doc["timeInOut"] = timeMicrocontroller;
        doc["timeInOutUser"] = timeNow;
        doc["version"] = "1";
        doc["deviceCode"] = "Card_" + userId;
        doc["macAddress"] = mac;
        doc["x"] = x;
        doc["y"] = y;
        doc["z"] = z;
        doc["batteryLevel"] = batteryLevel;
        doc["timeActivity"] = timeActivity;
        doc["Name"] = name;
 
        serializeJson(doc, json);
        Serial.printf("\n┌──────────────────────────────────\n");
        Serial.printf("│ 🌐 URL: %s/PostLocation\n", "http://brd-parque-it.pointservice.com.br/api/v1/IOT");
        Serial.printf("└──────────────────────────────────\n");

        if (http.begin("http://brd-parque-it.pointservice.com.br/api/v1/IOT/PostLocation")) {
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(15000);

            int httpCode = http.POST(json);
            Serial.printf("\n┌──────────────────────────────────\n");
            Serial.printf("│ 📡 Status: %d %s\n", httpCode, 
                        httpCode == 200 ? "✅ OK" : 
                        httpCode >= 500 ? "❌ Server Error" : 
                        httpCode >= 400 ? "⚠️ Request Error" : 
                        "⚡ Connection Error");
            Serial.printf("└──────────────────────────────────\n");

            String result = http.getString();
            DynamicJsonDocument payload(4776);
            deserializeJson(payload, result);
            Serial.println(result);
            _id = payload["_id"].as<String>();
            http.end();
        } else {
            Serial.println("Connection Error");
        }
    }
}

void Distributor::process() 
{
    if (millis() - inicioMedia > TIME_MEDIA) 
    {
        sending = true;
        Serial.println("\nStarting sending process");

        Serial.printf("\nallUsers.size: \n %d", users.size());

        for (int i = 0; i < users.size(); i++) 
        {
            Serial.println("\n-----------------------User postIn---------------------------");
            Serial.printf("\nDevice: %s\n", users[i].getId().c_str());
            Serial.printf("\nSending...(%d)\n", i+1);
            Serial.printf("📊 Leituras RSSI acumuladas: %d/3 (mínimo necessário)\n", users[i].getMediasRssi().size());

            loggedIn(i);
            if (users[i].isLoggedIn())
            {
                int mode = CalculateMode(users[i].getMediasRssi());
                double distance = CalculateDistance(mode*-1);
                users[i].clearMediasRssi();
                users[i].incrementVezes();
                
                // Calcular localização do beacon (se habilitado)
                double beaconLat = 0.0;
                double beaconLon = 0.0;
                if (ENABLE_GEOLOCATION) {
                    calculateBeaconLocation(distance, mode, beaconLat, beaconLon);
                }
                
                // MQTT: Enviar dados simplificados (flat) para ThingsBoard
                if (connect != nullptr && ENABLE_GEOLOCATION) {
                    DynamicJsonDocument doc(512);
                    
                    // Campos principais
                    doc["messageType"] = "postin";  // Para filtro do ThingsBoard
                    doc["type"] = "geoposition";    // Para processar geofences
                    doc["deviceId"] = DEVICE_ID;
                    doc["objectId"] = users[i].getMac();
                    
                    // Coordenadas
                    doc["lat"] = beaconLat;
                    doc["lng"] = beaconLon;
                    
                    // Dados do sensor
                    if (users[i].getBatteryLevel() > 0) {
                        doc["battery"] = users[i].getBatteryLevel();
                    }
                    
                    float tempX = users[i].getX();
                    float tempY = users[i].getY();
                    float tempZ = users[i].getZ();
                    
                    if (tempX != 0 || tempY != 0 || tempZ != 0) {
                        doc["accelerometerX"] = tempX;
                        doc["accelerometerY"] = tempY;
                        doc["accelerometerZ"] = tempZ;
                    }
                    
                    // Accuracy estimada
                    float accuracy = distance * 0.5;
                    if (accuracy < 2.0) accuracy = 2.0;
                    if (accuracy > 20.0) accuracy = 20.0;
                    doc["accuracy"] = accuracy;
                    
                    doc["signalPower"] = mode;
                    doc["distance"] = distance;
                    
                    // Metadados do ESP32
                    doc["espFirmwareVersion"] = FIRMWARE_VERSION;
                    doc["signalStrength"] = WiFi.RSSI();
                    doc["timestamp"] = connect->getEpochMillis();
                    
                    String jsonData;
                    serializeJson(doc, jsonData);
                    
                    Serial.println("\n📡 Enviando dados de localização:");
                    Serial.println(jsonData);
                    
                    connect->publishTelemetry(jsonData);
                }
                else if (connect != nullptr && !ENABLE_GEOLOCATION) {
                    // Formato antigo se geolocalização estiver desabilitada
                    String deviceCode = "Card_" + users[i].getId();
                    String macAddress = users[i].getMac().c_str();
                    
                    if (!validateDeviceCodeWithMAC(deviceCode, macAddress)) {
                        Serial.printf("⚠️ SALTANDO envio - DeviceCode não corresponde ao MAC: %s != %s\n", 
                                    deviceCode.c_str(), macAddress.c_str());
                        continue;
                    }
                    
                    DynamicJsonDocument doc(512);
                    doc["messageType"] = "postin";
                    doc["gatewayId"] = DEVICE_ID;
                    doc["deviceId"] = users[i].getId();
                    doc["mac"] = users[i].getMac();
                    doc["sinalPower"] = mode;
                    doc["batteryLevel"] = users[i].getBatteryLevel();
                    doc["distance"] = distance;
                    doc["ts"] = connect->getEpochMillis();

                    String jsonData;
                    serializeJson(doc, jsonData);
                    connect->publishTelemetry(jsonData);
                }
                delay(100);
            }
        }

        inicioMedia = millis();
        sending = false;
        Serial.printf("\nFINISH ENDING PROCESS\n");
    }
    else if (millis() - lastScanTime > SCAN_INTERVAL && BLEDevice::getInitialized() == true && !sending) 
    { 
        Serial.printf("\nTIME to ACTIVE: %d\n", millis() - lastSendTime);
        Serial.println("\nTESTE 2: START scan + ListDevices (processando dados)");
        
        if (advertisements != nullptr && BLEDevice::getInitialized() == true) {
          Serial.println("🔵 Iniciando BLE scan...");
          BLEScanResults* foundDevices = pBLEScan->start(3, false);
          Serial.printf("🔵 Scan completo! Encontrados: %d dispositivos\n", foundDevices ? foundDevices->getCount() : 0);
          if (foundDevices != nullptr) {
            Serial.println("🟡 Processando dispositivos com ListDevices()...");
            advertisements->ListDevices(*foundDevices);
            Serial.println("✅ ListDevices() completou com sucesso!");
          }
          pBLEScan->clearResults();
        }
        lastScanTime = millis();
    }
    else
    {
        if(sending)
            Serial.printf("\nIn sending process: sending == true\n");
        else {
            static const char spinner[] = {'|', '/', '-', '\\'};
            static int spinnerPos = 0;
            static unsigned long lastSpinnerUpdate = 0;
            
            if (millis() - lastSpinnerUpdate > 100) {
                char buffer[50];
                sprintf(buffer, "%c", spinner[spinnerPos]);
                Serial.print("\r");
                Serial.print(buffer);
                spinnerPos = (spinnerPos + 1) % 4;
                lastSpinnerUpdate = millis();
            }
        }
    }
}

// Novo método para enviar dados via MQTT para ThingsBoard
void Distributor::sendDataToThingsBoard(User& user) {
    if (connect == nullptr) {
        Serial.println("Erro: Connect não inicializado");
        return;
    }
    
    // Criar JSON com os dados do usuário
    const size_t capacity = JSON_OBJECT_SIZE(16) + 220;
    DynamicJsonDocument doc(capacity);
    
    doc["messageType"] = "aggregated_telemetry";
    doc["gatewayId"] = DEVICE_ID;
    doc["deviceId"] = user.getId();
    doc["mac"] = user.getMac();
    doc["rssi"] = user.getAnalog().getValue();
    doc["battery"] = user.getBatteryLevel();
    doc["distance"] = CalculateDistance(user.getAnalog().getValue());
    doc["x"] = user.getX();
    doc["y"] = user.getY();
    doc["z"] = user.getZ();
    doc["timeActivity"] = user.getTimeActivity();
    doc["deviceType"] = user.getDeviceTypeUser();
    doc["loggedIn"] = user.isLoggedIn();
    if (connect != nullptr) {
      doc["ts"] = connect->getEpochMillis();
    } else {
      doc["ts"] = millis();
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Enviar via MQTT
    if (connect->publishTelemetry(jsonString)) {
        Serial.println("✓ Dados enviados ao ThingsBoard via MQTT");
        Serial.println("  Payload: " + jsonString);
    } else {
        Serial.println("✗ Falha ao enviar dados ao ThingsBoard");
    }
}
