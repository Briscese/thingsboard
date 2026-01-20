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

// Configurações do Dispositivo
extern const char DEVICE_ID[];
extern const int WIFI_SIGNAL_LIMIT;
extern const int MAX_ERROR_MODE;

// Configurações MQTT - ThingsBoard
extern const char MQTT_SERVER[];
extern const int MQTT_PORT;
extern const char MQTT_TOKEN[];
extern const char MQTT_TOPIC[];

#endif