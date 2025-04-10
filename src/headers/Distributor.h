#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include <Arduino.h>
#include <vector>
#include "headers/User.h"
#include <BLEScan.h>
#include "headers/Advertisements.h"

class Distributor {
private:
    std::vector<User>& users;
    bool sending;
    unsigned long inicioMedia;
    unsigned long lastScanTime;
    unsigned long lastSendTime;
    int TIME_MEDIA;
    int SCAN_INTERVAL;
    BLEScan* pBLEScan;
    Advertisements* advertisements;

    void loggedIn(int pos);

public:
    // Construtor que recebe a referência para o vetor de usuários
    Distributor(std::vector<User>& users, int timeMedia, int scanInterval, BLEScan* pBLEScan);
    ~Distributor() { if (advertisements) delete advertisements; }
    
    // Método principal que executa o loop
    void process();

    // Métodos para gerenciamento de usuários
    int findUser(const String& id);
    void UserRegisterData(const String& macAddress, const String& code, int rssiBLE, 
                               int deviceType, int batterylevel, float x, float y, float z, 
                               float timeActivity);
    
    // Getters e setters
    bool isSending() const { return sending; }
    void setSending(bool value) { sending = value; }
    void setInicioMedia(unsigned long value) { inicioMedia = value; }
    void setLastScanTime(unsigned long value) { lastScanTime = value; }
    void setLastSendTime(unsigned long value) { lastSendTime = value; }
    unsigned long getLastSendTime() const { return lastSendTime; }
    std::vector<User>& getUsers() { return users; }
};

#endif // DISTRIBUTOR_H 