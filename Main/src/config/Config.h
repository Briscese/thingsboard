#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//*Aqui você deve alterar as configurações do WIFI para rodar de acordo com sua rede

// Configurações de WiFi
const char* WIFI_SSID = "pointservice";
const char* WIFI_PASSWORD = "HGgs@250";
const char* WIFI_SSID_ALTERNATIVE = "FAB&ITO1";
const char* WIFI_PASSWORD_ALTERNATIVE = "CidadeInteligente";

// Configurações do Servidor
const char* SERVER_PASSWORD = "ErrorLogServer123";
const String API_URL = "http://brd-parque-it.pointservice.com.br/api/v1/IOT";

// Configurações do Dispositivo
const char* DEVICE_ID = "FAB_SJC_CE1_T_0007";
const int WIFI_SIGNAL_LIMIT = -80;
const int MAX_ERROR_MODE = 1800000;

// Configurações MQTT - ThingsBoard
const char* MQTT_SERVER = "52.247.226.162";
const int MQTT_PORT = 1883;
const char* MQTT_TOKEN = "pAGDT3S1SFG8i4TytyNu";  // Token do dispositivo ThingsBoard
const char* MQTT_TOPIC = "v1/devices/me/telemetry";  // Tópico para enviar telemetria

#endif