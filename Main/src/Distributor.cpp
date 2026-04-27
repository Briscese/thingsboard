#include <BLEDevice.h>
#include "Distributor.h"
#include "Advertisements.h"
#include "Connect.h"
#include "config/Config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <cctype>

extern Connect* connect;

const int SCAN_INTERVAL = 5000;
const int TIME_MEDIA = 30000;  // 30 segundos - garante tempo para acumular 3+ leituras (scans a cada 3s)

static String buildHttpFallbackUrl(const String& url);

static void configureSecureClient(WiFiClientSecure& client) {
  client.setInsecure();
  client.setTimeout(15000);
}

Distributor::Distributor(std::vector<User>& users, BLEScan* pBLEScan, String api_url) 
    : users(users),
      sending(false),
      inicioMedia(0),
      lastScanTime(0),
      lastSendTime(0),
      lastTrackedRefresh(0),
      trackedRefreshInterval(TRACKED_OBJECTS_REFRESH_MS),
      lastBoardLocationRefresh(0),
      boardLocationRefreshInterval(TRACKED_OBJECTS_REFRESH_MS),
      pBLEScan(pBLEScan),
      advertisements(new Advertisements()),
      API_URL(api_url),
      hasBoardLocation(false),
      hasBoardAzimuth(false),
      boardLat(0.0),
      boardLng(0.0),
      boardAzimuth(0.0),
      connectivityDiagnosticsDone(false)
{
  if (strlen(AUTH_BEARER_TOKEN) == 0) {
    refreshBearerTokenFromMiddleware();
  }
  syncTrackedMacs(true);
  refreshBoardLocation(true);
}

void Distributor::runConnectivityDiagnostics() {
  if (connectivityDiagnosticsDone) {
    return;
  }
  connectivityDiagnosticsDone = true;

  if (!String(API_URL).startsWith("https://") && !String(MIDDLEWARE_LOGIN_URL).startsWith("https://")) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Diag conexao: WiFi nao conectado, pulando testes HTTPS");
    return;
  }

  Serial.println("=== Diag conexao HTTPS iniciado ===");

  {
    String healthUrl = API_URL;
    if (!healthUrl.endsWith("/")) {
      healthUrl += "/";
    }
    healthUrl += "health";

    HTTPClient request;
    request.setReuse(false);
    WiFiClientSecure secureClient;
    configureSecureClient(secureClient);

    if (request.begin(secureClient, healthUrl)) {
      request.setTimeout(15000);
      int statusCode = request.GET();
      Serial.print("Diag GET /health HTTP ");
      Serial.println(statusCode);
      if (statusCode < 0) {
        Serial.print("Diag GET /health erro: ");
        Serial.println(request.errorToString(statusCode).c_str());
      } else {
        String body = request.getString();
        Serial.print("Diag GET /health body: ");
        if (body.length() > 120) {
          Serial.println(body.substring(0, 120));
        } else {
          Serial.println(body);
        }
      }
      request.end();
    } else {
      Serial.println("Diag GET /health: falha no begin HTTPS");
    }
  }

  {
    String loginUrl = String(MIDDLEWARE_LOGIN_URL);
    HTTPClient request;
    request.setReuse(false);
    WiFiClientSecure secureClient;
    configureSecureClient(secureClient);

    if (request.begin(secureClient, loginUrl)) {
      request.setTimeout(15000);
      request.addHeader("Content-Type", "application/json");
      request.addHeader("accept", "text/plain");

      StaticJsonDocument<256> body;
      body["username"] = MIDDLEWARE_USERNAME;
      body["password"] = MIDDLEWARE_PASSWORD;
      String loginPayload;
      serializeJson(body, loginPayload);

      int statusCode = request.POST(loginPayload);
      Serial.print("Diag POST /user/login HTTP ");
      Serial.println(statusCode);
      if (statusCode < 0) {
        Serial.print("Diag POST /user/login erro: ");
        Serial.println(request.errorToString(statusCode).c_str());
      } else {
        String response = request.getString();
        Serial.print("Diag POST /user/login body: ");
        if (response.length() > 220) {
          Serial.println(response.substring(0, 220));
        } else {
          Serial.println(response);
        }
      }
      request.end();
    } else {
      Serial.println("Diag POST /user/login: falha no begin HTTPS");
    }
  }

  Serial.println("=== Diag conexao HTTPS fim ===");
}

bool Distributor::refreshBearerTokenFromMiddleware() {
  if (strlen(MIDDLEWARE_LOGIN_URL) == 0 || strlen(MIDDLEWARE_USERNAME) == 0 || strlen(MIDDLEWARE_PASSWORD) == 0) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient request;
  request.setReuse(false);
  String loginUrl = String(MIDDLEWARE_LOGIN_URL);
  WiFiClientSecure secureClient;
  if (loginUrl.startsWith("https://")) {
    configureSecureClient(secureClient);
    if (!request.begin(secureClient, loginUrl)) {
      Serial.println("Falha ao iniciar login HTTPS no middleware");
      return false;
    }
  } else if (!request.begin(loginUrl)) {
    Serial.println("Falha ao iniciar login no middleware");
    return false;
  }

  request.setTimeout(15000);
  request.addHeader("Content-Type", "application/json");
  request.addHeader("accept", "text/plain");

  StaticJsonDocument<256> body;
  body["username"] = MIDDLEWARE_USERNAME;
  body["password"] = MIDDLEWARE_PASSWORD;
  String loginPayload;
  serializeJson(body, loginPayload);

  int statusCode = request.POST(loginPayload);
  if (statusCode < 0 && loginUrl.startsWith("https://")) {
    request.end();
    String fallbackUrl = buildHttpFallbackUrl(loginUrl);
    HTTPClient fallbackRequest;
    fallbackRequest.setReuse(false);
    if (fallbackRequest.begin(fallbackUrl)) {
      fallbackRequest.setTimeout(15000);
      fallbackRequest.addHeader("Content-Type", "application/json");
      fallbackRequest.addHeader("accept", "text/plain");
      statusCode = fallbackRequest.POST(loginPayload);
      Serial.print("Fallback login URL: ");
      Serial.println(fallbackUrl);
      if (statusCode == HTTP_CODE_OK) {
        String responseBody = fallbackRequest.getString();
        fallbackRequest.end();

        DynamicJsonDocument response(4096);
        DeserializationError error = deserializeJson(response, responseBody);
        if (error) {
          Serial.print("Erro parse login middleware (fallback): ");
          Serial.println(error.c_str());
          return false;
        }

        if (!response["content"]["data"]["token"].is<const char*>()) {
          Serial.println("Login middleware fallback sem token");
          return false;
        }

        runtimeBearerToken = response["content"]["data"]["token"].as<const char*>();
        if (runtimeBearerToken.length() == 0) {
          return false;
        }

        Serial.println("Token middleware atualizado na placa (fallback HTTP)");
        return true;
      }
      fallbackRequest.end();
    }
  }

  if (statusCode != HTTP_CODE_OK) {
    Serial.print("Falha no login middleware. HTTP ");
    Serial.println(statusCode);
    if (statusCode < 0) {
      Serial.print("Detalhe login middleware: ");
      Serial.println(request.errorToString(statusCode).c_str());
    }
    request.end();
    return false;
  }

  String responseBody = request.getString();
  request.end();

  DynamicJsonDocument response(4096);
  DeserializationError error = deserializeJson(response, responseBody);
  if (error) {
    Serial.print("Erro parse login middleware: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!response["content"]["data"]["token"].is<const char*>()) {
    Serial.println("Login middleware sem token");
    return false;
  }

  runtimeBearerToken = response["content"]["data"]["token"].as<const char*>();
  if (runtimeBearerToken.length() == 0) {
    return false;
  }

  Serial.println("Token middleware atualizado na placa");
  return true;
}

void Distributor::addBackendAuthHeaders(HTTPClient& request) {
  if (strlen(API_KEY_VALUE) > 0) {
    request.addHeader(API_KEY_HEADER, API_KEY_VALUE);
  }

  String bearer = "";
  if (strlen(AUTH_BEARER_TOKEN) > 0) {
    bearer = String(AUTH_BEARER_TOKEN);
  } else if (runtimeBearerToken.length() > 0) {
    bearer = runtimeBearerToken;
  }

  if (bearer.length() > 0) {
    request.addHeader(AUTH_HEADER, String("Bearer ") + bearer);
  }

  if (strlen(TENANT_VALUE) > 0) {
    request.addHeader(TENANT_HEADER, TENANT_VALUE);
  }
}

static String normalizeMacAddress(const String& macAddress) {
  String normalized = "";
  normalized.reserve(12);
  for (size_t i = 0; i < macAddress.length(); i++) {
    char c = macAddress[i];
    if (isxdigit(static_cast<unsigned char>(c))) {
      normalized += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  return normalized;
}

static String formatMacAddressForLog(const String& normalizedMac) {
  if (normalizedMac.length() != 12) {
    return normalizedMac;
  }

  String formatted = "";
  formatted.reserve(17);
  for (int i = 0; i < 12; i += 2) {
    if (i > 0) {
      formatted += ':';
    }
    formatted += normalizedMac.substring(i, i + 2);
  }
  return formatted;
}

static String buildHttpFallbackUrl(const String& url) {
  if (url.startsWith("https://")) {
    return String("http://") + url.substring(8);
  }
  return String("");
}

int Distributor::getTrackedBatteryLevel(const String& normalizedMac) {
  for (const User& user : users) {
    String userMac = normalizeMacAddress(user.getMac());
    if (userMac == normalizedMac) {
      int batteryLevel = user.getBatteryLevel();
      if (batteryLevel > 0) {
        for (size_t i = 0; i < trackedBatteryMacs.size(); i++) {
          if (trackedBatteryMacs[i] == normalizedMac) {
            trackedBatteryValues[i] = batteryLevel;
            return batteryLevel;
          }
        }
        trackedBatteryMacs.push_back(normalizedMac);
        trackedBatteryValues.push_back(batteryLevel);
        return batteryLevel;
      }
      break;
    }
  }

  for (size_t i = 0; i < trackedBatteryMacs.size(); i++) {
    if (trackedBatteryMacs[i] == normalizedMac) {
      return trackedBatteryValues[i];
    }
  }

  return -1;
}

void Distributor::updateTrackedBatteryLevel(const String& macAddress, int batteryLevel) {
  if (batteryLevel <= 0) {
    return;
  }

  String normalizedMac = normalizeMacAddress(macAddress);
  if (normalizedMac.length() != 12) {
    return;
  }

  for (size_t i = 0; i < trackedBatteryMacs.size(); i++) {
    if (trackedBatteryMacs[i] == normalizedMac) {
      trackedBatteryValues[i] = batteryLevel;
      return;
    }
  }

  trackedBatteryMacs.push_back(normalizedMac);
  trackedBatteryValues.push_back(batteryLevel);
}

int Distributor::applyTrackedRssiSmoothing(const String& normalizedMac, int rawRssi) {
  const float alpha = 0.35f;

  for (size_t i = 0; i < smoothedRssiMacs.size(); i++) {
    if (smoothedRssiMacs[i] == normalizedMac) {
      float smoothed = (alpha * static_cast<float>(rawRssi)) +
                       ((1.0f - alpha) * smoothedRssiValues[i]);
      smoothedRssiValues[i] = smoothed;
      return static_cast<int>(roundf(smoothed));
    }
  }

  smoothedRssiMacs.push_back(normalizedMac);
  smoothedRssiValues.push_back(static_cast<float>(rawRssi));
  return rawRssi;
}

void Distributor::publishTrackedMacRssiTelemetry(const String& normalizedMac, int rssi) {
  if (connect == nullptr) {
    return;
  }

  refreshBoardLocation(false);

  int trackedRssiToSend = rssi;
  if (rssi != -127) {
    trackedRssiToSend = applyTrackedRssiSmoothing(normalizedMac, rssi);
  }

  StaticJsonDocument<768> doc;
  doc["messageType"] = "tracked_rssi";
  doc["deviceId"] = DEVICE_ID;
  doc["espDeviceId"] = DEVICE_ID;
  doc["trackedMac"] = formatMacAddressForLog(normalizedMac);
  doc["trackedRssi"] = trackedRssiToSend;
  int batteryLevel = getTrackedBatteryLevel(normalizedMac);
  if (batteryLevel >= 0) {
    doc["trackedBatteryLevel"] = batteryLevel;
  }
  doc["wifiRssi"] = WiFi.RSSI();
  doc["uptime"] = millis();
  doc["status"] = "online";
  if (hasBoardLocation) {
    doc["latESP"] = boardLat;
    doc["lngESP"] = boardLng;
  }
  if (hasBoardAzimuth) {
    doc["azimuthESP"] = boardAzimuth;
  }

  String jsonData;
  serializeJson(doc, jsonData);
  if (doc.overflowed()) {
    Serial.println("Falha ao montar telemetria RSSI: JSON sem capacidade");
    return;
  }
  connect->publishTelemetry(jsonData);
  connect->loopMQTT();
  delay(25);
}

String Distributor::buildTrackedObjectsUrl() const {
  String url = API_URL;
  String endpoint = TRACKED_OBJECTS_ENDPOINT;

  if (endpoint.length() == 0) {
    return url;
  }

  bool urlEndsWithSlash = url.endsWith("/");
  bool endpointStartsWithSlash = endpoint.startsWith("/");

  if (urlEndsWithSlash && endpointStartsWithSlash) {
    url.remove(url.length() - 1);
  } else if (!urlEndsWithSlash && !endpointStartsWithSlash) {
    url += "/";
  }

  url += endpoint;
  return url;
}

String Distributor::buildBoardLocationUrl() const {
  String url = API_URL;
  const String endpointPrefix = "/boards-esp/";

  if (!url.endsWith("/")) {
    url += "/";
  }

  if (endpointPrefix.startsWith("/")) {
    url.remove(url.length() - 1);
  }

  url += endpointPrefix;
  url += DEVICE_ID;
  return url;
}

bool Distributor::refreshBoardLocation(bool forceRefresh) {
  if (!forceRefresh && (millis() - lastBoardLocationRefresh) < boardLocationRefreshInterval) {
    return hasBoardLocation;
  }

  lastBoardLocationRefresh = millis();

  if (WiFi.status() != WL_CONNECTED) {
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  String url = buildBoardLocationUrl();
  HTTPClient request;
  request.setReuse(false);
  WiFiClientSecure secureClient;
  if (url.startsWith("https://")) {
    configureSecureClient(secureClient);
    if (!request.begin(secureClient, url)) {
      Serial.println("Falha ao iniciar requisicao HTTPS para localizacao da placa");
      boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
      return false;
    }
  } else if (!request.begin(url)) {
    Serial.println("Falha ao iniciar requisicao HTTP para localizacao da placa");
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  request.setTimeout(15000);
  addBackendAuthHeaders(request);

  int statusCode = request.GET();
  if (statusCode == HTTP_CODE_UNAUTHORIZED && strlen(AUTH_BEARER_TOKEN) == 0) {
    request.end();
    runtimeBearerToken = "";
    if (refreshBearerTokenFromMiddleware()) {
      HTTPClient retryRequest;
      retryRequest.setReuse(false);
      bool retryBeginOk = false;
      WiFiClientSecure retrySecureClient;
      if (url.startsWith("https://")) {
        configureSecureClient(retrySecureClient);
        retryBeginOk = retryRequest.begin(retrySecureClient, url);
      } else {
        retryBeginOk = retryRequest.begin(url);
      }
      if (retryBeginOk) {
        retryRequest.setTimeout(15000);
        addBackendAuthHeaders(retryRequest);
        statusCode = retryRequest.GET();
        if (statusCode == HTTP_CODE_OK) {
          String payload = retryRequest.getString();
          retryRequest.end();

          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, payload);
          if (error) {
            Serial.print("Erro ao parsear JSON de localizacao da placa: ");
            Serial.println(error.c_str());
            boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
            return false;
          }

          if (!doc["lat"].is<float>() && !doc["lat"].is<double>()) {
            boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
            return false;
          }
          if (!doc["lng"].is<float>() && !doc["lng"].is<double>()) {
            boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
            return false;
          }

          boardLat = doc["lat"].as<double>();
          boardLng = doc["lng"].as<double>();
          hasBoardLocation = true;

          if (doc["azimuth"].is<float>() || doc["azimuth"].is<double>() || doc["azimuth"].is<int>()) {
            boardAzimuth = doc["azimuth"].as<double>();
            hasBoardAzimuth = true;
          }

          boardLocationRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;

          Serial.print("Localizacao da placa atualizada: latESP=");
          Serial.print(boardLat, 6);
          Serial.print(" lngESP=");
          Serial.print(boardLng, 6);
          if (hasBoardAzimuth) {
            Serial.print(" azimuthESP=");
            Serial.println(boardAzimuth, 2);
          } else {
            Serial.println();
          }
          return true;
        }
        retryRequest.end();
      }
    }
  }

  String payload = "";
  if (statusCode < 0 && url.startsWith("https://")) {
    request.end();
    String fallbackUrl = buildHttpFallbackUrl(url);
    HTTPClient fallbackRequest;
    fallbackRequest.setReuse(false);
    if (fallbackUrl.length() > 0 && fallbackRequest.begin(fallbackUrl)) {
      fallbackRequest.setTimeout(15000);
      addBackendAuthHeaders(fallbackRequest);
      statusCode = fallbackRequest.GET();
      Serial.print("Fallback localizacao URL: ");
      Serial.println(fallbackUrl);
      if (statusCode == HTTP_CODE_OK) {
        payload = fallbackRequest.getString();
      }
      fallbackRequest.end();
    }
  }

  if (statusCode != HTTP_CODE_OK) {
    Serial.print("Falha ao buscar localizacao da placa. HTTP ");
    Serial.println(statusCode);
    if (statusCode < 0) {
      Serial.print("Detalhe localizacao placa: ");
      Serial.println(request.errorToString(statusCode).c_str());
    }
    request.end();
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  if (payload.length() == 0) {
    payload = request.getString();
  }
  request.end();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("Erro ao parsear JSON de localizacao da placa: ");
    Serial.println(error.c_str());
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  if (!doc["lat"].is<float>() && !doc["lat"].is<double>()) {
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }
  if (!doc["lng"].is<float>() && !doc["lng"].is<double>()) {
    boardLocationRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  boardLat = doc["lat"].as<double>();
  boardLng = doc["lng"].as<double>();
  hasBoardLocation = true;

  if (doc["azimuth"].is<float>() || doc["azimuth"].is<double>() || doc["azimuth"].is<int>()) {
    boardAzimuth = doc["azimuth"].as<double>();
    hasBoardAzimuth = true;
  }

  boardLocationRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;

  Serial.print("Localizacao da placa atualizada: latESP=");
  Serial.print(boardLat, 6);
  Serial.print(" lngESP=");
  Serial.print(boardLng, 6);
  if (hasBoardAzimuth) {
    Serial.print(" azimuthESP=");
    Serial.println(boardAzimuth, 2);
  } else {
    Serial.println();
  }
  return true;
}

bool Distributor::refreshTrackedMacs(bool forceRefresh) {
  runConnectivityDiagnostics();

  if (!forceRefresh && (millis() - lastTrackedRefresh) < trackedRefreshInterval) {
    return true;
  }

  lastTrackedRefresh = millis();

  if (WiFi.status() != WL_CONNECTED) {
    trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  String url = buildTrackedObjectsUrl();
  Serial.print("Sincronizando MACs em: ");
  Serial.println(url);
  Serial.print("WiFi localIP: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi gateway: ");
  Serial.println(WiFi.gatewayIP());
  String apiKeyPreview = String(API_KEY_VALUE);
  String tenantPreview = String(TENANT_VALUE);
  Serial.print("Header ");
  Serial.print(API_KEY_HEADER);
  Serial.print(" len=");
  Serial.println(apiKeyPreview.length());
  if (apiKeyPreview.length() >= 6) {
    Serial.print("API key preview: ");
    Serial.print(apiKeyPreview.substring(0, 3));
    Serial.println("***");
  }
  if (tenantPreview.length() > 0) {
    Serial.print("Tenant enviado: ");
    Serial.println(tenantPreview);
  }
  HTTPClient request;
  request.setReuse(false);
  WiFiClientSecure secureClient;
  if (url.startsWith("https://")) {
    configureSecureClient(secureClient);
    if (!request.begin(secureClient, url)) {
      Serial.println("Falha ao iniciar requisicao HTTPS para lista de MACs");
      trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
      return false;
    }
  } else if (!request.begin(url)) {
    Serial.println("Falha ao iniciar requisicao HTTP para lista de MACs");
    trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  request.setTimeout(15000);
  addBackendAuthHeaders(request);

  int statusCode = request.GET();
  String payload = "";
  if (statusCode == HTTP_CODE_UNAUTHORIZED && strlen(AUTH_BEARER_TOKEN) == 0) {
    request.end();
    runtimeBearerToken = "";
    if (refreshBearerTokenFromMiddleware()) {
      HTTPClient retryRequest;
      retryRequest.setReuse(false);
      bool retryBeginOk = false;
      WiFiClientSecure retrySecureClient;
      if (url.startsWith("https://")) {
        configureSecureClient(retrySecureClient);
        retryBeginOk = retryRequest.begin(retrySecureClient, url);
      } else {
        retryBeginOk = retryRequest.begin(url);
      }
      if (retryBeginOk) {
        retryRequest.setTimeout(15000);
        addBackendAuthHeaders(retryRequest);
        statusCode = retryRequest.GET();
        if (statusCode == HTTP_CODE_OK) {
          String payload = retryRequest.getString();
          retryRequest.end();

          DynamicJsonDocument doc(16384);
          DeserializationError error = deserializeJson(doc, payload);
          if (error) {
            Serial.print("Erro ao parsear JSON de MACs: ");
            Serial.println(error.c_str());
            trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
            return false;
          }

          std::vector<String> newTrackedMacs;
          int totalItems = 0;
          int skippedBoardMismatch = 0;
          int skippedEmptyMac = 0;
          int skippedInvalidMac = 0;
          int skippedDuplicate = 0;

          JsonArray objectsArray;
          if (doc.is<JsonArray>()) {
            objectsArray = doc.as<JsonArray>();
          } else if (doc.is<JsonObject>() && doc["data"].is<JsonArray>()) {
            objectsArray = doc["data"].as<JsonArray>();
          }

          if (!objectsArray.isNull()) {
            for (JsonVariant item : objectsArray) {
              totalItems++;
              String mac = "";
              bool boardMatches = true;
              String boardEspId = "";

              if (item.is<JsonObject>()) {
                JsonObject object = item.as<JsonObject>();

                if (object["boardEspId"].is<const char*>()) {
                  boardEspId = object["boardEspId"].as<const char*>();
                  if (boardEspId.length() > 0 && !boardEspId.equalsIgnoreCase(DEVICE_ID)) {
                    boardMatches = false;
                  }
                }

                if (object["id"].is<const char*>()) {
                  mac = object["id"].as<const char*>();
                } else if (object["mac"].is<const char*>()) {
                  mac = object["mac"].as<const char*>();
                } else if (object["macAddress"].is<const char*>()) {
                  mac = object["macAddress"].as<const char*>();
                }
              } else if (item.is<const char*>()) {
                mac = item.as<const char*>();
              }

              if (!boardMatches) {
                skippedBoardMismatch++;
                continue;
              }

              if (mac.length() == 0) {
                skippedEmptyMac++;
                continue;
              }

              String normalized = normalizeMacAddress(mac);
              if (normalized.length() != 12) {
                skippedInvalidMac++;
                continue;
              }

              bool alreadyAdded = false;
              for (const String& tracked : newTrackedMacs) {
                if (tracked == normalized) {
                  alreadyAdded = true;
                  break;
                }
              }

              if (!alreadyAdded) {
                newTrackedMacs.push_back(normalized);
              } else {
                skippedDuplicate++;
              }
            }
          }

          Serial.print("Resumo sync MACs: total=");
          Serial.print(totalItems);
          Serial.print(" aceitos=");
          Serial.print(static_cast<int>(newTrackedMacs.size()));
          Serial.print(" boardMismatch=");
          Serial.print(skippedBoardMismatch);
          Serial.print(" semMac=");
          Serial.print(skippedEmptyMac);
          Serial.print(" formatoInvalido=");
          Serial.print(skippedInvalidMac);
          Serial.print(" duplicados=");
          Serial.println(skippedDuplicate);

          if (!newTrackedMacs.empty()) {
            trackedMacs = newTrackedMacs;
            trackedRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;
            Serial.print("MACs rastreados atualizados: ");
            Serial.println(static_cast<int>(trackedMacs.size()));
            for (const String& tracked : trackedMacs) {
              Serial.print(" - MAC permitido: ");
              Serial.println(tracked);
            }
            return true;
          }

          Serial.println("Lista de MACs rastreados vazia. Mantendo filtro atual.");
          trackedRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;
          return false;
        }
        retryRequest.end();
      }
    }
  }

  if (statusCode < 0 && url.startsWith("https://")) {
    request.end();
    String fallbackUrl = buildHttpFallbackUrl(url);
    HTTPClient fallbackRequest;
    fallbackRequest.setReuse(false);
    if (fallbackUrl.length() > 0 && fallbackRequest.begin(fallbackUrl)) {
      fallbackRequest.setTimeout(15000);
      addBackendAuthHeaders(fallbackRequest);
      statusCode = fallbackRequest.GET();
      Serial.print("Fallback objetos URL: ");
      Serial.println(fallbackUrl);
      if (statusCode == HTTP_CODE_OK) {
        payload = fallbackRequest.getString();
      }
      fallbackRequest.end();
    }
  }

  if (statusCode != HTTP_CODE_OK) {
    Serial.print("Falha ao buscar MACs rastreados. HTTP ");
    Serial.println(statusCode);
    if (statusCode < 0) {
      Serial.print("Detalhe HTTPClient: ");
      Serial.println(request.errorToString(statusCode).c_str());
    }
    String errorBody = request.getString();
    if (errorBody.length() > 0) {
      Serial.print("Resposta da API: ");
      if (errorBody.length() > 180) {
        Serial.println(errorBody.substring(0, 180));
      } else {
        Serial.println(errorBody);
      }
    }
    request.end();
    trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  if (payload.length() == 0) {
    payload = request.getString();
  }
  request.end();

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("Erro ao parsear JSON de MACs: ");
    Serial.println(error.c_str());
    trackedRefreshInterval = TRACKED_OBJECTS_RETRY_ERROR_MS;
    return false;
  }

  std::vector<String> newTrackedMacs;
  int totalItems = 0;
  int skippedBoardMismatch = 0;
  int skippedEmptyMac = 0;
  int skippedInvalidMac = 0;
  int skippedDuplicate = 0;

  JsonArray objectsArray;
  if (doc.is<JsonArray>()) {
    objectsArray = doc.as<JsonArray>();
  } else if (doc.is<JsonObject>() && doc["data"].is<JsonArray>()) {
    objectsArray = doc["data"].as<JsonArray>();
  }

  if (!objectsArray.isNull()) {
    for (JsonVariant item : objectsArray) {
      totalItems++;
      String mac = "";
      bool boardMatches = true;
      String boardEspId = "";

      if (item.is<JsonObject>()) {
        JsonObject object = item.as<JsonObject>();

        if (object["boardEspId"].is<const char*>()) {
          boardEspId = object["boardEspId"].as<const char*>();
          if (boardEspId.length() > 0 && !boardEspId.equalsIgnoreCase(DEVICE_ID)) {
            boardMatches = false;
          }
        }

        if (object["id"].is<const char*>()) {
          mac = object["id"].as<const char*>();
        } else if (object["mac"].is<const char*>()) {
          mac = object["mac"].as<const char*>();
        } else if (object["macAddress"].is<const char*>()) {
          mac = object["macAddress"].as<const char*>();
        }
      } else if (item.is<const char*>()) {
        mac = item.as<const char*>();
      }

      if (!boardMatches) {
        skippedBoardMismatch++;
        Serial.print("MAC descartado por boardEspId. item.boardEspId=");
        Serial.print(boardEspId);
        Serial.print(" deviceIdEsperado=");
        Serial.println(DEVICE_ID);
        continue;
      }

      if (mac.length() == 0) {
        skippedEmptyMac++;
        continue;
      }

      String normalized = normalizeMacAddress(mac);
      if (normalized.length() != 12) {
        skippedInvalidMac++;
        Serial.print("MAC descartado por formato invalido: ");
        Serial.println(mac);
        continue;
      }

      bool alreadyAdded = false;
      for (const String& tracked : newTrackedMacs) {
        if (tracked == normalized) {
          alreadyAdded = true;
          break;
        }
      }

      if (!alreadyAdded) {
        newTrackedMacs.push_back(normalized);
      } else {
        skippedDuplicate++;
      }
    }
  }

  Serial.print("Resumo sync MACs: total=");
  Serial.print(totalItems);
  Serial.print(" aceitos=");
  Serial.print(static_cast<int>(newTrackedMacs.size()));
  Serial.print(" boardMismatch=");
  Serial.print(skippedBoardMismatch);
  Serial.print(" semMac=");
  Serial.print(skippedEmptyMac);
  Serial.print(" formatoInvalido=");
  Serial.print(skippedInvalidMac);
  Serial.print(" duplicados=");
  Serial.println(skippedDuplicate);

  if (!newTrackedMacs.empty()) {
    trackedMacs = newTrackedMacs;
    trackedRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;
    Serial.print("MACs rastreados atualizados: ");
    Serial.println(static_cast<int>(trackedMacs.size()));
    for (const String& tracked : trackedMacs) {
      Serial.print(" - MAC permitido: ");
      Serial.println(tracked);
    }
    return true;
  }

  Serial.println("Lista de MACs rastreados vazia. Mantendo filtro atual.");
  trackedRefreshInterval = TRACKED_OBJECTS_REFRESH_MS;
  return false;
}

void Distributor::syncTrackedMacs(bool forceRefresh) {
  refreshTrackedMacs(forceRefresh);
}

bool Distributor::isTrackedMac(const String& macAddress) const {
  String normalized = normalizeMacAddress(macAddress);
  if (normalized.length() != 12) {
    return false;
  }

  for (const String& tracked : trackedMacs) {
    if (tracked == normalized) {
      return true;
    }
  }
  return false;
}

double CalculateDistance(double signalPower){
  if(signalPower==0)return 0.0;
  const double n=2.0,A=-60;
  double B=0.0;
  switch((int)signalPower){
    case -90 ... -88:
      B = 2.5;  // 10m+
      break;
    case -87 ... -85:
      B = 2.1;  // 8-10m (suavizado)
      break;
    case -84 ... -82:
      B = 1.6;  // 5-7m (reduzido para suavizar transição)
      break;
    case -81 ... -79:
      B = 1.1;  // 3-5m (reduzido para pular 2m)
      break;
    case -78 ... -76:
      B = 0.7;  // 2.5-3m (reduzido para suavizar)
      break;
    case -75 ... -73:
      B = 0.5;  // ~3m (região 3m limpa)
      break;
    case -72 ... -70:
      B = 0.2;  // 1-2m (suavizado)
      break;
    case -69 ... -67:
      B = 0.0;  // ~1m (região 1m limpa)
      break;
    case -66 ... -64:
      B = 0.0;  // ~1m
      break;
    default:
      B = 0.0;
      break;
  }  
  double distance = pow(10, (A - signalPower) / (10 * n)) + B;

  return distance;
}

int CalculateMode(const std::vector<int>& numbers) {
    if (numbers.empty()) {
      return 0;
    }

    int mostRepeated = 0;
    int repeat = 0;
    int counter = 0;

    for (size_t i = 0; i < numbers.size(); i++) {
      mostRepeated = numbers[0];
      repeat = 0;
      counter = 0;

      if (i + 1 < numbers.size() && numbers[i] == numbers[i + 1]) {
        repeat++;
      } else {
        repeat = 1;
      }

      if (repeat > counter) {
        counter = repeat;
        mostRepeated = numbers[i];
      }
    }

    return mostRepeated;
}

// ============================================================
// FUNÇÕES DE GEOLOCALIZAÇÃO
// ============================================================

/**
 * Estima um ângulo (em graus) baseado no RSSI para simular direção do beacon
 * Em uma implementação real, você poderia usar múltiplos gateways ou antenas 
 * para triangulação. Aqui, usamos o RSSI como base para variação angular.
 */
double Distributor::estimateAngleFromRSSI(int rssi) {
    // Usa o RSSI para criar variação no ângulo (0-360 graus)
    // Quanto mais forte o sinal (RSSI mais próximo de 0), mais próximo de norte (0°)
    // Sinais fracos distribuem em outras direções
    
    // Normalizar RSSI (-90 a -40) para (0 a 360 graus)
    double normalized = ((rssi + 90.0) / 50.0) * 360.0;
    
    // Garantir que está entre 0-360
    if (normalized < 0) normalized = 0;
    if (normalized > 360) normalized = 360;
    
    return normalized;
}

/**
 * Calcula latitude e longitude estimada do beacon baseado na distância e RSSI
 * Usa a localização fixa do gateway como referência
 * 
 * @param distance Distância em metros calculada a partir do RSSI
 * @param rssi Valor do RSSI em dBm
 * @param latitude (OUT) Latitude calculada do beacon
 * @param longitude (OUT) Longitude calculada do beacon
 */
void Distributor::calculateBeaconLocation(double distance, int rssi, double& latitude, double& longitude) {
    // Localização fixa do gateway (definida em Config.h)
    double gatewayLat = GATEWAY_LATITUDE;
    double gatewayLon = GATEWAY_LONGITUDE;
    
    // Estimar ângulo baseado no RSSI (em graus)
    double angle = estimateAngleFromRSSI(rssi);
    if (USE_FIXED_AZIMUTH) {
      angle = fmod(FIXED_AZIMUTH_DEGREES, 360.0);
      if (angle < 0.0) {
        angle += 360.0;
      }
    }
    
    // Converter ângulo para radianos
    double angleRad = angle * (M_PI / 180.0);
    
    // Raio da Terra em metros
    const double EARTH_RADIUS = 6371000.0;
    
    // Calcular deslocamento em latitude e longitude
    // Fórmula simplificada para distâncias curtas (< 100m)
    double deltaLat = (distance * cos(angleRad)) / EARTH_RADIUS * (180.0 / M_PI);
    double deltaLon = (distance * sin(angleRad)) / (EARTH_RADIUS * cos(gatewayLat * M_PI / 180.0)) * (180.0 / M_PI);
    
    // Calcular posição final do beacon
    latitude = gatewayLat + deltaLat;
    longitude = gatewayLon + deltaLon;
}

void Distributor::loggedIn(int pos) {
  if (users[pos].getMediasRssi().size() >= 3) {
    users[pos].setLoggedIn(true);
  }
  else {
    users[pos].setLoggedIn(false);
    users[pos].setVezes(0);
  }
}

int Distributor::findUser(const String& id) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].getId() == id) {
            return i;
        }
    }
    return -1;
}

// Validar se o deviceCode corresponde aos últimos 4 caracteres do MAC address
bool Distributor::validateDeviceCodeWithMAC(const String& deviceCode, const String& macAddress) {
    if (deviceCode.length() < 4 || macAddress.length() < 2) {
        return false;
    }
    
    // Extrair últimos 4 caracteres do MAC (sem os ":")
    // Exemplo: macAddress = "84:CC:A8:2C:72:30" -> extrair "72:30" -> "7230"
    String macLast4 = macAddress.substring(macAddress.length() - 5);
    String macClean = "";
    for (int i = 0; i < macLast4.length(); i++) {
        if (macLast4[i] != ':') {
            macClean += macLast4[i];
        }
    }
    macClean.toUpperCase();
    
    // Extrair últimos 4 caracteres do código (ex: "Card_595C" -> "595C")
    String codeClean = deviceCode.substring(deviceCode.length() - 4);
    codeClean.toUpperCase();
    
    if (macClean == codeClean) {
        return true;
    } else {
        return false;
    }
}

void Distributor::UserRegisterData(const std::string& macAddress, const std::string& code, int rssiBLE, 
                                   int deviceType, int batterylevel, float x, float y, float z, 
                                   float timeActivity, String name) {
    if (users.empty()) {
        User firstUser;
        firstUser.setId(code.c_str());
        firstUser.setBatteryLevel(batterylevel);
        firstUser.setX(x);
        firstUser.setY(y);
        firstUser.setZ(z);
        firstUser.updateAnalog((rssiBLE * -1));
        firstUser.addMediaRssi(firstUser.getAnalog().getValue());
        firstUser.setMac(macAddress.c_str());
        firstUser.setTimeActivity(timeActivity);
        firstUser.setDeviceTypeUser(deviceType);
        users.push_back(firstUser);
    } else {
        int foundUser = findUser(code.c_str());
        if (foundUser != -1) {
            if(users[foundUser].getBatteryLevel() == 0 && batterylevel > 0) {
                users[foundUser].setBatteryLevel(batterylevel);
            }
            users[foundUser].setX(x);
            users[foundUser].setY(y);
            users[foundUser].setZ(z);
            users[foundUser].updateAnalog((rssiBLE * -1));
            users[foundUser].addMediaRssi(users[foundUser].getAnalog().getValue());
            users[foundUser].setTimeActivity(timeActivity);
        } else {
            User newUser;
            newUser.setId(code.c_str());
            newUser.setBatteryLevel(batterylevel);
            newUser.setX(x);
            newUser.setY(y);
            newUser.setZ(z);
            newUser.updateAnalog((rssiBLE * -1));
            newUser.addMediaRssi(newUser.getAnalog().getValue());
            newUser.setMac(macAddress.c_str());
            newUser.setDeviceTypeUser(deviceType);
            newUser.setTimeActivity(timeActivity);
            users.push_back(newUser);
        }
    }
}

void Distributor::postIn(String userId, int media, String tempo, String mac, 
                    int deviceType, int batteryLevel, float x, float y, float z, 
                    float timeActivity, String name, int mode, double distance) {
    if (connect->validateStatusWIFI()) {
        String json;
        const size_t capacity = JSON_OBJECT_SIZE(25);
        DynamicJsonDocument doc(capacity);

        const char* timeNow = tempo.c_str();
        String sendTime = connect->getTime();
        const char* timeMicrocontroller = sendTime.c_str();

        doc["userCode"] = userId.toInt();
        doc["microcontrollerId"] = connect->getSendId();
        doc["typeEvent"] = 1;
        doc["sinalPower"] = media;
        doc["timeInOut"] = timeMicrocontroller;
        doc["timeInOutUser"] = timeNow;
        doc["version"] = "1";
        doc["deviceCode"] = "Card_" + userId;
        doc["macAddress"] = mac;
        doc["x"] = x;
        doc["y"] = y;
        doc["z"] = z;
        doc["batteryLevel"] = batteryLevel;
        doc["timeActivity"] = timeActivity;
        doc["Name"] = name;
 
        serializeJson(doc, json);

        if (http.begin("http://brd-parque-it.pointservice.com.br/api/v1/IOT/PostLocation")) {
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(15000);

            int httpCode = http.POST(json);

            String result = http.getString();
            DynamicJsonDocument payload(4776);
            deserializeJson(payload, result);
            _id = payload["_id"].as<String>();
            http.end();
        }
    }
}

void Distributor::process() 
{
  syncTrackedMacs(false);

    if (millis() - inicioMedia > TIME_MEDIA) 
    {
        sending = true;

        for (int i = 0; i < users.size(); i++) 
        {
            loggedIn(i);
            if (users[i].isLoggedIn())
            {
                int mode = CalculateMode(users[i].getMediasRssi());
                users[i].clearMediasRssi();
                users[i].incrementVezes();
            publishTrackedMacRssiTelemetry(normalizeMacAddress(users[i].getMac()), -mode);
                delay(100);
            }
        }

        inicioMedia = millis();
        sending = false;
    }
    else if (millis() - lastScanTime > SCAN_INTERVAL && BLEDevice::getInitialized() == true && !sending) 
    { 
        if (advertisements != nullptr && BLEDevice::getInitialized() == true) {
          Serial.print("Heap livre antes do scan BLE: ");
          Serial.println(ESP.getFreeHeap());
          BLEScanResults* foundDevices = pBLEScan->start(1, false);
          Serial.print("Heap livre depois do scan BLE: ");
          Serial.println(ESP.getFreeHeap());
          if (foundDevices != nullptr) {
            std::vector<int> trackedRssi;
            if (!trackedMacs.empty()) {
              const int rssiNotFound = 127;
              trackedRssi.assign(trackedMacs.size(), rssiNotFound);
              int totalFound = foundDevices->getCount();

              for (int i = 0; i < totalFound; i++) {
                BLEAdvertisedDevice device = foundDevices->getDevice(i);
                String scannedMac = String(device.getAddress().toString().c_str());
                String normalizedScannedMac = normalizeMacAddress(scannedMac);
                int scannedRssi = device.getRSSI();

                for (size_t trackedIndex = 0; trackedIndex < trackedMacs.size(); trackedIndex++) {
                  if (trackedMacs[trackedIndex] == normalizedScannedMac) {
                    trackedRssi[trackedIndex] = scannedRssi;
                  }
                }
              }
            }

            Serial.println("🟡 Processando dispositivos com ListDevices()...");
            advertisements->ListDevices(*foundDevices);
            Serial.println("✅ ListDevices() completou com sucesso!");
            pBLEScan->clearResults();
            Serial.print("Heap livre apos liberar scan BLE: ");
            Serial.println(ESP.getFreeHeap());

            if (!trackedMacs.empty() && trackedRssi.size() == trackedMacs.size()) {
              const int rssiNotFound = 127;
              Serial.println("RSSI dos MACs rastreados:");
              for (size_t trackedIndex = 0; trackedIndex < trackedMacs.size(); trackedIndex++) {
                Serial.print(" - ");
                Serial.print(formatMacAddressForLog(trackedMacs[trackedIndex]));
                Serial.print(": ");

                if (trackedRssi[trackedIndex] == rssiNotFound) {
                  Serial.println("nao encontrado neste scan");
                  publishTrackedMacRssiTelemetry(trackedMacs[trackedIndex], -127);
                } else {
                  Serial.print(trackedRssi[trackedIndex]);
                  Serial.println(" dBm");
                  publishTrackedMacRssiTelemetry(trackedMacs[trackedIndex], trackedRssi[trackedIndex]);
                }
              }
            }
          } else {
            pBLEScan->clearResults();
          }
        }
        lastScanTime = millis();
    }
    else
    {
        if(sending)
            Serial.printf("\nIn sending process: sending == true\n");
        else {
            static const char spinner[] = {'|', '/', '-', '\\'};
            static int spinnerPos = 0;
            static unsigned long lastSpinnerUpdate = 0;
            
            if (millis() - lastSpinnerUpdate > 100) {
                char buffer[50];
                sprintf(buffer, "%c", spinner[spinnerPos]);
                Serial.print("\r");
                Serial.print(buffer);
                spinnerPos = (spinnerPos + 1) % 4;
                lastSpinnerUpdate = millis();
            }
        }
    }
}

// Novo método para enviar dados via MQTT para ThingsBoard
void Distributor::sendDataToThingsBoard(User& user) {
    if (connect == nullptr) {
        Serial.println("Erro: Connect não inicializado");
        return;
    }
    
    // Criar JSON com os dados do usuário
    const size_t capacity = JSON_OBJECT_SIZE(16) + 220;
    DynamicJsonDocument doc(capacity);
    
    doc["messageType"] = "aggregated_telemetry";
    doc["gatewayId"] = DEVICE_ID;
    doc["deviceId"] = user.getId();
    doc["mac"] = user.getMac();
    doc["rssi"] = user.getAnalog().getValue();
    doc["battery"] = user.getBatteryLevel();
    doc["distance"] = CalculateDistance(user.getAnalog().getValue());
    doc["x"] = user.getX();
    doc["y"] = user.getY();
    doc["z"] = user.getZ();
    doc["timeActivity"] = user.getTimeActivity();
    doc["deviceType"] = user.getDeviceTypeUser();
    doc["loggedIn"] = user.isLoggedIn();
    if (connect != nullptr) {
      doc["ts"] = connect->getEpochMillis();
    } else {
      doc["ts"] = millis();
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Enviar via MQTT
    if (connect->publishTelemetry(jsonString)) {
        Serial.println("✓ Dados enviados ao ThingsBoard via MQTT");
        Serial.println("  Payload: " + jsonString);
    } else {
        Serial.println("✗ Falha ao enviar dados ao ThingsBoard");
    }
}
