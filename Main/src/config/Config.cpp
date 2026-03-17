#include "Config.h"

const char WIFI_SSID[] = "FAB&ITO1";
const char WIFI_PASSWORD[] = "CidadeInteligente";
//const char WIFI_SSID[] = "CLARO_2G16D7C2";
//const char WIFI_PASSWORD[] = "3816D7C2";
const char WIFI_SSID_ALTERNATIVE[] = "FAB&ITO1";
const char WIFI_PASSWORD_ALTERNATIVE[] = "CidadeInteligente";

const char SERVER_PASSWORD[] = "ErrorLogServer123";
// Keep HTTPS endpoint as baseline; firmware can fallback to HTTP when needed.
const String API_URL = "https://api.location-service.pointservice.com.br";
const char TRACKED_OBJECTS_ENDPOINT[] = "/objects";
const char API_KEY_HEADER[] = "X-Api-Key";
const char API_KEY_VALUE[] = "2ED26FDF-32ED-4FC1-BC36-66018DBC7F40";
const char AUTH_HEADER[] = "Authorization";
const char AUTH_BEARER_TOKEN[] = "";
const char MIDDLEWARE_LOGIN_URL[] = "https://mid-dev-assets.pointservice.com.br/api/v1/user/login";
const char MIDDLEWARE_USERNAME[] = "adm";
const char MIDDLEWARE_PASSWORD[] = "adm";
const char TENANT_HEADER[] = "X-Tenant-Id";
const char TENANT_VALUE[] = "ad038b35-5540-40e6-bbc0-9230e74b7b34";
const unsigned long TRACKED_OBJECTS_REFRESH_MS = 60000;
const unsigned long TRACKED_OBJECTS_RETRY_ERROR_MS = 10000;

const char DEVICE_ID[] = "POC_0001";
const char FIRMWARE_VERSION[] = "1.0.2";  // Versão do firmware ESP32
const int WIFI_SIGNAL_LIMIT = -80;
const int MAX_ERROR_MODE = 1800000;

const int MQTT_PORT = 1883;
const char MQTT_SERVER[] = "thingsboard.iot8.com.br";
const char MQTT_TOKEN[] = "bgaUWs4uV0io0f2358Q9";
const char MQTT_TOPIC[] = "v1/devices/me/telemetry";


const double GATEWAY_LATITUDE = -23.157161;
const double GATEWAY_LONGITUDE = -45.790537;
const bool ENABLE_GEOLOCATION = true;
const char TARGET_BEACON_MAC[] = "E4:1E:F1:8C:59:5C";
const bool USE_FIXED_AZIMUTH = true;
const double FIXED_AZIMUTH_DEGREES = 45.0;



