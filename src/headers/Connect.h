#ifndef CONNECT_H
#define CONNECT_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Preferences.h>

class Connect {
public:
    Connect(const char* name, const char* password, 
            const char* alternative_name, const char* alternative_password,
            const char* server_pwd, int wifi_limit, int max_error_mode);
    
    void activeSoftAP();
    void updatePreferences();
    bool validateStatusWIFI();
    void loadErrorMode();
    void getOn(String s);
    void updateBoard(String url);
    String getTime() const { return timeClient.getFormattedTime(); }
    String getSendId() const { return sendId; }
    bool isErrorMode() const { return errorMode; }
    bool isSending() const { return sending; }

private:
    WiFiServer server;
    WiFiUDP ntpUDP;
    NTPClient timeClient;
    HTTPClient http;
    Preferences preferences;
    
    const char* NAME;
    const char* PASSWORD;
    const char* ALTERNATIVE_NAME;
    const char* ALTERNATIVE_PASSWORD;
    const char* SERVER_PASSWORD;
    const int WIFI_LIMIT;
    const int MAX_ERROR_MODE;
    
    bool errorMode;
    bool sending;
    int attempts;
    int communicationErrors;
    unsigned long lastSendTime;
    unsigned long initErrorMode;
    String sendId;
    String header;
};

#endif
