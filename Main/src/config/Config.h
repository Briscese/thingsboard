#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//*Aqui você deve alterar as configurações do WIFI para rodar de acordo com sua rede

// Configurações de WiFi
const char* WIFI_SSID = "CantinhodaBELEZASUPERIOR";
const char* WIFI_PASSWORD = "05012715";
const char* WIFI_SSID_ALTERNATIVE = "FAB&ITO1";
const char* WIFI_PASSWORD_ALTERNATIVE = "CidadeInteligente";

// Configurações do Servidor
const char* SERVER_PASSWORD = "ErrorLogServer123";
const String API_URL = "http://brd-parque-it.pointservice.com.br/api/v1/IOT";

// Configurações do Dispositivo
const char* DEVICE_ID = "FAB_SJC_CE1_T_0007";
const int WIFI_SIGNAL_LIMIT = -80;
const int MAX_ERROR_MODE = 1800000;

#endif