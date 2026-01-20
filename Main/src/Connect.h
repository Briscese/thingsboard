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
#include <PubSubClient.h>

class Connect {
public:
    Connect(const char* name, const char* password, 
            const char* alternative_name, const char* alternative_password,
            const char* server_pwd, int wifi_limit, int max_error_mode, const String api_url,
            const char* mqtt_server, int mqtt_port, const char* mqtt_token, const char* mqtt_topic);
    
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
    bool syncTime();
    uint64_t getEpochMillis();
    bool hasSyncedTime() const { return timeSynced; }
    
    // Métodos MQTT
    bool connectMQTT();
    bool publishTelemetry(const String& jsonData);
    bool publishTelemetryRaw(const char* data, size_t length = 0);
    bool isMQTTConnected();
    void loopMQTT();

private:
    WiFiServer server;
    WiFiUDP ntpUDP;
    NTPClient timeClient;
    HTTPClient http;
    Preferences preferences;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    
    const char* NAME;
    const char* PASSWORD;
    const char* ALTERNATIVE_NAME;
    const char* ALTERNATIVE_PASSWORD;
    const char* SERVER_PASSWORD;
    const int WIFI_LIMIT;
    const int MAX_ERROR_MODE;
    const String API_URL;
    
    // Configurações MQTT
    const char* MQTT_SERVER;
    const int MQTT_PORT;
    const char* MQTT_TOKEN;
    const char* MQTT_TOPIC;
    
    bool errorMode;
    bool sending;
    int attempts;
    int communicationErrors;
    unsigned long lastSendTime;
    unsigned long initErrorMode;
    String sendId;
    String header;
    bool timeSynced;
    const long TIME_OFFSET_SECONDS;
    bool timeClientStarted;
};

#endif
