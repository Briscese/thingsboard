#ifndef USER_H
#define USER_H

#include <Arduino.h>
#include <ResponsiveAnalogRead.h>
#include <vector>

class User {
private:
    String id;
    String name;
    std::vector<int> mediasRssi;
    bool loggedIn;
    int batteryLevel;
    float x;
    float y;
    float z;
    int vezes;
    String tempo;
    String mac;
    int deviceTypeUser;
    float timeActivity;
    ResponsiveAnalogRead analog;

    static std::vector<User> allUsers;

public:
    User();
    
    // Getters existentes
    String getId() const { return id; }
    String getName() const { return name; }
    const std::vector<int>& getMediasRssi() const { return mediasRssi; }
    bool isLoggedIn() const { return loggedIn; }
    int getBatteryLevel() const { return batteryLevel; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    int getVezes() const { return vezes; }
    String getTempo() const { return tempo; }
    String getMac() const { return mac; }
    int getDeviceTypeUser() const { return deviceTypeUser; }
    float getTimeActivity() const { return timeActivity; }
    ResponsiveAnalogRead& getAnalog() { return analog; }

    // Setters existentes
    void setId(const String& value) { id = value; }
    void setName(String value) { name = value; }
    void addMediaRssi(int value) { mediasRssi.push_back(value); }
    void clearMediasRssi() { mediasRssi.clear(); }
    void setLoggedIn(bool value) { loggedIn = value; }
    void setBatteryLevel(int value) { batteryLevel = value; }
    void setX(float value) { x = value; }
    void setY(float value) { y = value; }
    void setZ(float value) { z = value; }
    void incrementVezes() { vezes++; }
    void setVezes(int value) { vezes = value; }
    void setTempo(const String& value) { tempo = value; }
    void setMac(const String& value) { mac = value; }
    void setDeviceTypeUser(int value) { deviceTypeUser = value; }
    void setTimeActivity(float value) { timeActivity = value; }
    void updateAnalog(int value) { analog.update(value); }

    // Métodos estáticos para gerenciar todos os usuários
    static std::vector<User>& getAllUsers() { return allUsers; }
    static void clearAllUsers() { allUsers.clear(); }
};

#endif
