#include "headers/Distributor.h"
#include <BLEDevice.h>

extern void postIn(String userId, int media, String tempo, String mac, int deviceType, int batteryLevel, float x, float y, float z, float timeActivity);
extern void ListDevices();

Distributor::Distributor(std::vector<User>& users, int timeMedia, int scanInterval) 
    : users(users),
      sending(false),
      inicioMedia(0),
      lastScanTime(0),
      lastSendTime(0),
      TIME_MEDIA(timeMedia),
      SCAN_INTERVAL(scanInterval)
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

void Distributor::process() 
{
    if (millis() - inicioMedia > TIME_MEDIA) 
    {
        sending = true;
        Serial.println("\nIniciando processo de envio");
        int i = 0;

        Serial.printf("\nallUsers.size: \n %d", users.size(),"\n");

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
            i++;
        }

        inicioMedia = millis();
        sending = false;
        Serial.printf("\nFINISH ENDING PROCESS\n");
    }
    else if (millis() - lastScanTime > SCAN_INTERVAL && BLEDevice::getInitialized() == true && sending == false) 
    { 
        Serial.printf("\nTIME to ACTIVE: %d\n", millis() - lastSendTime);
        Serial.println("\nSCAN BLE...");
        ListDevices();
    }
    else
    {
        if(sending)
            Serial.printf("\nEm processo de envio: sending == true\n");
        Serial.printf("\n.");
    }
} 