#include "Config.h"

const char WIFI_SSID[] = "FAB&ITO1";
const char WIFI_PASSWORD[] = "CidadeInteligente";
const char WIFI_SSID_ALTERNATIVE[] = "FAB&ITO1";
const char WIFI_PASSWORD_ALTERNATIVE[] = "CidadeInteligente";

const char SERVER_PASSWORD[] = "ErrorLogServer123";
const String API_URL = "http://192.168.1.192:5081";
const char TRACKED_OBJECTS_ENDPOINT[] = "/objects";
const char API_KEY_HEADER[] = "X-Api-Key";
const char API_KEY_VALUE[] = "demo-api-key-pointservice";
const char TENANT_HEADER[] = "X-Tenant-Id";
const char TENANT_VALUE[] = "ad038b35-5540-40e6-bbc0-9230e74b7b34";
const unsigned long TRACKED_OBJECTS_REFRESH_MS = 60000;
const unsigned long TRACKED_OBJECTS_RETRY_ERROR_MS = 10000;

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

const char TARGET_BEACON_MAC[] = "E4:1E:F1:8C:59:5C";
const bool USE_FIXED_AZIMUTH = true;
const double FIXED_AZIMUTH_DEGREES = 90.0;
