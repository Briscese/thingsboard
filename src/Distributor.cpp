#include "headers/Distributor.h"
#include <BLEDevice.h>
#include "headers/Advertisements.h"
#include "headers/Connect.h"

extern Connect* connect;

Distributor::Distributor(std::vector<User>& users, int timeMedia, int scanInterval, BLEScan* pBLEScan) 
    : users(users),
      sending(false),
      inicioMedia(0),
      lastScanTime(0),
      lastSendTime(0),
      TIME_MEDIA(timeMedia),
      SCAN_INTERVAL(scanInterval),
      pBLEScan(pBLEScan),
      advertisements(new Advertisements())
{
}

double CalculateDistance(double sinalPower)
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

int CalculateMode(const std::vector<int>& numbers) {
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

void Distributor::UserRegisterData(const String& macAddress, const String& code, int rssiBLE, 
                                       int deviceType, int batterylevel, float x, float y, float z, 
                                       float timeActivity) {
    if (users.empty()) {
        User firstUser;
        firstUser.setId(code);
        firstUser.setBatteryLevel(batterylevel);
        firstUser.setX(x);
        firstUser.setY(y);
        firstUser.setZ(z);
        firstUser.updateAnalog((rssiBLE * -1));
        firstUser.addMediaRssi(firstUser.getAnalog().getValue());
        firstUser.setMac(macAddress);
        firstUser.setTimeActivity(timeActivity);
        firstUser.setDeviceTypeUser(deviceType);
        users.push_back(firstUser);
    } else {
        int foundUser = findUser(code);
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
            newUser.setId(code);
            newUser.setBatteryLevel(batterylevel);
            newUser.setX(x);
            newUser.setY(y);
            newUser.setZ(z);
            newUser.updateAnalog((rssiBLE * -1));
            newUser.addMediaRssi(newUser.getAnalog().getValue());
            newUser.setMac(macAddress);
            newUser.setDeviceTypeUser(deviceType);
            newUser.setTimeActivity(timeActivity);
            users.push_back(newUser);
        }
    }
}

void Distributor::postIn(String userId, int media, String tempo, String mac, 
                    int deviceType, int batteryLevel, float x, float y, float z, 
                    float timeActivity) {
    Serial.printf("\n📡 POST para API - Dispositivo %s:\n", mac);
    Serial.printf("┌──────────────────────────────────\n");
    Serial.printf("│ 👤 Usuário: %s\n", userId);
    Serial.printf("│ 📊 Média RSSI: %d\n", media);
    Serial.printf("│ ⏰ Tempo: %s\n", tempo);
    Serial.printf("│ 📱 MAC: %s\n", mac);
    Serial.printf("│ 🔢 Tipo: %d\n", deviceType);
    Serial.printf("│ 🔋 Bateria: %d%%\n", batteryLevel);
    if (deviceType == 4) {
        Serial.printf("│ 🎯 Acelerômetro:\n");
        Serial.printf("│   ➡️ X: %.2f\n", x);
        Serial.printf("│   ⬆️ Y: %.2f\n", y);
        Serial.printf("│   ↗️ Z: %.2f\n", z);
    }
    Serial.printf("│ ⏱️ Tempo Ativo: %.1f dias\n", timeActivity);
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
                        httpCode >= 500 ? "❌ Erro no Servidor" : 
                        httpCode >= 400 ? "⚠️ Erro na Requisição" : 
                        "⚡ Erro na Conexão");
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

void Distributor::process() 
{
    if (millis() - inicioMedia > TIME_MEDIA) 
    {
        sending = true;
        Serial.println("\nIniciando processo de envio");

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
                CalculateDistance(mode*-1);
                users[i].clearMediasRssi();
                users[i].incrementVezes();
                
                postIn(users[i].getId(), mode, users[i].getTempo(), users[i].getMac(), 
                      users[i].getDeviceTypeUser(), users[i].getBatteryLevel(), 
                      users[i].getX(), users[i].getY(), users[i].getZ(), 
                      users[i].getTimeActivity());
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
        Serial.println("\nSCAN BLE...");
        
        if (advertisements != nullptr) {
            BLEScanResults foundDevices = pBLEScan->start(3);
            advertisements->ListDevices(foundDevices);
            pBLEScan->clearResults();
            lastScanTime = millis();
        }
    }
    else
    {
        if(sending)
            Serial.printf("\nEm processo de envio: sending == true\n");
        else {
            static const char spinner[] = {'|', '/', '-', '\\'};
            static int spinnerPos = 0;
            Serial.printf("\r[%c] Procurando dispositivos...", spinner[spinnerPos]);
            spinnerPos = (spinnerPos + 1) % 4;
        }
    }
} 