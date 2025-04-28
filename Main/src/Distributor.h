#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include <vector>
#include <BLEDevice.h>
#include "models/User.h"
#include "Advertisements.h"
#include <HTTPClient.h>

class Distributor {
private:
    std::vector<User>& users;
    bool sending;
    unsigned long inicioMedia;
    unsigned long lastScanTime;
    unsigned long lastSendTime;
    BLEScan* pBLEScan;
    Advertisements* advertisements;
    HTTPClient http;
    String _id;

    void loggedIn(int pos);

public:
    Distributor(std::vector<User>& users, BLEScan* pBLEScan);
    void process();
    int findUser(const String& id);
    void UserRegisterData(const std::string& macAddress, const std::string& code, int rssiBLE, 
                         int deviceType, int batterylevel, float x, float y, float z, 
                         float timeActivity, String frameType, const std::string& bleuuid);
    void postIn(String userId, int media, String tempo, String mac, 
               int deviceType, int batteryLevel, float x, float y, float z, 
               float timeActivity, String frameType, String bleuuid);
    
    bool isSending() const { return sending; }
    void setSending(bool value) { sending = value; }
    void setInicioMedia(unsigned long value) { inicioMedia = value; }
    void setLastScanTime(unsigned long value) { lastScanTime = value; }
    void setLastSendTime(unsigned long value) { lastSendTime = value; }
    unsigned long getLastSendTime() const { return lastSendTime; }
    std::vector<User>& getUsers() { return users; }
};

#endif // DISTRIBUTOR_H