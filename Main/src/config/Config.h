#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//*Aqui você deve alterar as configurações do WIFI para rodar de acordo com sua rede

// Configurações de WiFi
extern const char WIFI_SSID[];
extern const char WIFI_PASSWORD[];
extern const char WIFI_SSID_ALTERNATIVE[];
extern const char WIFI_PASSWORD_ALTERNATIVE[];

// Configurações do Servidor
extern const char SERVER_PASSWORD[];
extern const String API_URL;
extern const char TRACKED_OBJECTS_ENDPOINT[];
extern const char API_KEY_HEADER[];
extern const char API_KEY_VALUE[];
extern const char AUTH_HEADER[];
extern const char AUTH_BEARER_TOKEN[];
extern const char MIDDLEWARE_LOGIN_URL[];
extern const char MIDDLEWARE_USERNAME[];
extern const char MIDDLEWARE_PASSWORD[];
extern const char TENANT_HEADER[];
extern const char TENANT_VALUE[];
extern const unsigned long TRACKED_OBJECTS_REFRESH_MS;
extern const unsigned long TRACKED_OBJECTS_RETRY_ERROR_MS;

// Configurações do Dispositivo
extern const char DEVICE_ID[];
extern const char FIRMWARE_VERSION[];  // Versão do firmware
extern const int WIFI_SIGNAL_LIMIT;
extern const int MAX_ERROR_MODE;

// Configurações MQTT - ThingsBoard
extern const char MQTT_SERVER[];
extern const int MQTT_PORT;
extern const char MQTT_TOKEN[];
extern const char MQTT_TOPIC[];

// Configurações de Localização Fixa do Gateway
extern const double GATEWAY_LATITUDE;   // Latitude fixa do gateway (ESP32)
extern const double GATEWAY_LONGITUDE;  // Longitude fixa do gateway (ESP32)
extern const bool ENABLE_GEOLOCATION;   // Habilitar cálculo de lat/long para beacons

// Configurações de direção (azimute)
extern const char TARGET_BEACON_MAC[];     // MAC alvo para filtro do scanner
extern const bool USE_FIXED_AZIMUTH;       // true = usa azimute fixo
extern const double FIXED_AZIMUTH_DEGREES; // 0=Norte, 90=Leste, 180=Sul, 270=Oeste

#endif