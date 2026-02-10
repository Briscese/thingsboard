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

double CalculateDistance(double signalPower){
  if(signalPower==0)return 0.0;
  const double n=2.0,A=-60;
  double B=0.0;
  switch((int)signalPower){
    case -90 ... -88:
      B = 2.5;  // 10m+
      break;
    case -87 ... -85:
      B = 2.1;  // 8-10m (suavizado)
      break;
    case -84 ... -82:
      B = 1.6;  // 5-7m (reduzido para suavizar transição)
      break;
    case -81 ... -79:
      B = 1.1;  // 3-5m (reduzido para pular 2m)
      break;
    case -78 ... -76:
      B = 0.7;  // 2.5-3m (reduzido para suavizar)
      break;
    case -75 ... -73:
      B = 0.5;  // ~3m (região 3m limpa)
      break;
    case -72 ... -70:
      B = 0.2;  // 1-2m (suavizado)
      break;
    case -69 ... -67:
      B = 0.0;  // ~1m (região 1m limpa)
      break;
    case -66 ... -64:
      B = 0.0;  // ~1m
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
}

void Distributor::loggedIn(int pos) {
  if (users[pos].getMediasRssi().size() >= 3) {
    users[pos].setLoggedIn(true);
  }
  else {
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
    
    if (macClean == codeClean) {
        return true;
    } else {
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

        if (http.begin("http://brd-parque-it.pointservice.com.br/api/v1/IOT/PostLocation")) {
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(15000);

            int httpCode = http.POST(json);

            String result = http.getString();
            DynamicJsonDocument payload(4776);
            deserializeJson(payload, result);
            _id = payload["_id"].as<String>();
            http.end();
        }
    }
}

void Distributor::process() 
{
    if (millis() - inicioMedia > TIME_MEDIA) 
    {
        sending = true;

        for (int i = 0; i < users.size(); i++) 
        {
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
                    
                    doc["signalPower"] = -mode;  // Sempre negativo!
                    doc["distance"] = distance;
                    
                    // Metadados do ESP32
                    doc["espFirmwareVersion"] = FIRMWARE_VERSION;
                    doc["signalStrength"] = WiFi.RSSI();
                    doc["timestamp"] = connect->getEpochMillis();
                    
                    String jsonData;
                    serializeJson(doc, jsonData);
                    
                    connect->publishTelemetry(jsonData);
                }
                else if (connect != nullptr && !ENABLE_GEOLOCATION) {
                    // Formato antigo se geolocalização estiver desabilitada
                    String deviceCode = "Card_" + users[i].getId();
                    String macAddress = users[i].getMac().c_str();
                    
                    if (!validateDeviceCodeWithMAC(deviceCode, macAddress)) {
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
    }
    else if (millis() - lastScanTime > SCAN_INTERVAL && BLEDevice::getInitialized() == true && !sending) 
    { 
        if (advertisements != nullptr && BLEDevice::getInitialized() == true) {
          BLEScanResults* foundDevices = pBLEScan->start(3, false);
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
