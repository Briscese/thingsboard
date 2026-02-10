#include "Config.h"

const char WIFI_SSID[] = "pointservice";
const char WIFI_PASSWORD[] = "HGgs@250";
const char WIFI_SSID_ALTERNATIVE[] = "FAB&ITO1";
const char WIFI_PASSWORD_ALTERNATIVE[] = "CidadeInteligente";

const char SERVER_PASSWORD[] = "ErrorLogServer123";
const String API_URL = "http://brd-dev.pointservice.com.br/api/v1/IOT";

const char DEVICE_ID[] = "POCM2_localhost";
const char FIRMWARE_VERSION[] = "1.0.2";  // Versão do firmware ESP32
const int WIFI_SIGNAL_LIMIT = -80;
const int MAX_ERROR_MODE = 1800000;

const int MQTT_PORT = 1883;
const char MQTT_SERVER[] = "52.247.226.162";
const char MQTT_TOKEN[] = "f8g9YWaIiKIfyG5KmRsn";
const char MQTT_TOPIC[] = "v1/devices/me/telemetry";


const double GATEWAY_LATITUDE = -23.157125;   
const double GATEWAY_LONGITUDE = -45.790524;  
const bool ENABLE_GEOLOCATION = true;         
