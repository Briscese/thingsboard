#ifndef CONNECT_H
#define CONNECT_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPUpdate.h>

class Connect {
private:
    WiFiServer server;
    HTTPClient http;
    Preferences preferences;
    WiFiUDP ntpUDP;
    NTPClient timeClient;
    
    // Constantes
    const char* NOME;
    const char* SENHA;
    const char* NOME_ALTERNATIVA;
    const char* SENHA_ALTERNATIVA;
    const char* server_password;
    int WIFI_LIMIT;
    int MAX_ERROR_MODE;
    
    // Variáveis de estado
    bool errorMode;
    bool sending;
    String header;
    String sendId;
    int tentativas;
    int comunicationErrors;
    uint32_t lastSendTime;
    uint32_t initErrorMode;
    
    // Métodos privados
    void activeSoftAP();
    void updatePreferences();
    
public:
    Connect(const char* nome, const char* senha, 
            const char* nome_alternativa, const char* senha_alternativa,
            const char* server_pwd, int wifi_limit, int max_error_mode);
    
    bool validateStatusWIFI();
    void postIn(String userId, int media, String tempo, String mac, 
                int deviceType, int batteryLevel, float x, float y, float z, 
                float timeActivity);
    void getOn(String s);
    void updatePlaca(String url);
    void loadErrorMode();
    
    // Getters
    bool isErrorMode() const { return errorMode; }
    bool isSending() const { return sending; }
    String getSendId() const { return sendId; }
    String getTime() { 
        timeClient.forceUpdate();
        return timeClient.getFormattedTime();
    }
};

#endif // CONNECT_H
