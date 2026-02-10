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
    String API_URL;

    void loggedIn(int pos);

public:
    Distributor(std::vector<User>& users, BLEScan* pBLEScan, String api_url);
    void process();
    int findUser(const String& id);
    bool validateDeviceCodeWithMAC(const String& deviceCode, const String& macAddress);
    void UserRegisterData(const std::string& macAddress, const std::string& code, int rssiBLE, 
                         int deviceType, int batterylevel, float x, float y, float z, 
                         float timeActivity,  String name);
    void postIn(String userId, int media, String tempo, String mac, 
               int deviceType, int batteryLevel, float x, float y, float z, 
               float timeActivity, String name, int mode, double distance);
    void sendDataToThingsBoard(User& user);  // Novo método para MQTT
    
    // Funções de Geolocalização
    void calculateBeaconLocation(double distance, int rssi, double& latitude, double& longitude);
    double estimateAngleFromRSSI(int rssi);
    
    bool isSending() const { return sending; }
    void setSending(bool value) { sending = value; }
    void setInicioMedia(unsigned long value) { inicioMedia = value; }
    void setLastScanTime(unsigned long value) { lastScanTime = value; }
    void setLastSendTime(unsigned long value) { lastSendTime = value; }
    unsigned long getLastSendTime() const { return lastSendTime; }
    std::vector<User>& getUsers() { return users; }
};

#endif // DISTRIBUTOR_H