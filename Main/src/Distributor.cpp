#include <BLEDevice.h>
#include "Distributor.h"
#include "Advertisements.h"
#include "Connect.h"
#include "config/Config.h"

extern Connect* connect;

const int SCAN_INTERVAL = 3000;
const int TIME_MEDIA = 100000;

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

  const double n = 2.8; 
  const double A = -60;
  double B = 0.0;

  switch ((int)signalPower) {
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

            loggedIn(i);
            if (users[i].isLoggedIn())
            {
                int mode = CalculateMode(users[i].getMediasRssi());
                double distance = CalculateDistance(mode*-1);
                users[i].clearMediasRssi();
                users[i].incrementVezes();
                
                postIn(users[i].getId(), mode, users[i].getTempo(), users[i].getMac(), 
                      users[i].getDeviceTypeUser(), users[i].getBatteryLevel(), 
                      users[i].getX(), users[i].getY(), users[i].getZ(), 
                      users[i].getTimeActivity(), users[i].getName(), mode, distance);
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