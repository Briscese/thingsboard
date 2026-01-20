#include "Connect.h"

Connect::Connect(const char* name, const char* password, 
                 const char* alternative_name, const char* alternative_password,
                 const char* server_pwd, int wifi_limit, int max_error_mode, String api_url,
                                 const char* mqtt_server, int mqtt_port, const char* mqtt_token, const char* mqtt_topic)
    : server(80),
      timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000),
      mqttClient(wifiClient),
      NAME(name),
      PASSWORD(password),
      ALTERNATIVE_NAME(alternative_name),
      ALTERNATIVE_PASSWORD(alternative_password),
      SERVER_PASSWORD(server_pwd),
      WIFI_LIMIT(wifi_limit),
      MAX_ERROR_MODE(max_error_mode),
      API_URL(api_url),
      MQTT_SERVER(mqtt_server),
      MQTT_PORT(mqtt_port),
      MQTT_TOKEN(mqtt_token),
      MQTT_TOPIC(mqtt_topic),
      errorMode(false),
      sending(false),
      attempts(0),
      communicationErrors(0),
      lastSendTime(0),
              initErrorMode(0),
              timeSynced(false),
              TIME_OFFSET_SECONDS(-10800),
              timeClientStarted(false)
{
    updatePreferences();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(1024);
}

void Connect::activeSoftAP() {
    if (errorMode) {
        initErrorMode = millis();
        preferences.begin("my-app", false);
        String s = preferences.getString("idPlaca");
        delay(50);
        if (s == "") {
            s = WiFi.macAddress();
        }
        preferences.end();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(s.c_str(), SERVER_PASSWORD);
        IPAddress IP = WiFi.softAPIP();
        server.begin();
    }
}

void Connect::updatePreferences() {
    preferences.begin("my-app", false);
    String s = preferences.getString("idPlaca");
    delay(500);

    if (s == "") {
        sendId = "FAB_SJC_CE1_T_0007";
        preferences.putString("idPlaca", sendId);
    }
    else if (sendId != "") {
        if (sendId == s) {
            sendId = s;
        } else {
            Serial.println("Updating board name... (RESET)");
            preferences.putString("idPlaca", sendId);
        }
    } else {
        sendId = s;
    }
    preferences.end();
}

bool Connect::validateStatusWIFI() {
    if (WiFi.RSSI() < WIFI_LIMIT) {
        Serial.println("WiFi Status: Disconnected (Low Power)");
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
        
        String json;
        const size_t capacity = JSON_OBJECT_SIZE(8);
        DynamicJsonDocument doc(capacity);
        doc["rssiWifi"] = WiFi.RSSI();
        serializeJson(doc, json);

        preferences.begin("my-app", false);
        String s = preferences.getString("errors", "yyy");
        int errors = preferences.getInt("errorCount", 0);
        if (s == "yyy") {
            preferences.putString("errors", "Error;Data<br>WiFiError_Out_Range;" + json + "<br>");
            preferences.putInt("errorCount", errors + 1);
        } else {
            String saving = s + "WiFiError_Out_Range;" + json + "<br>";
            preferences.putString("errors", saving);
            preferences.putInt("errorCount", errors + 1);
        }
        
        if (errors > 10 && errors < 15) {
            errorMode = true;
            activeSoftAP();
        } else if (errors > 15) {
            preferences.putInt("errorCount", 0);
            preferences.putString("errors", "");
        }
        preferences.end();
        
        WiFi.disconnect();
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Status: Disconnected");
        WiFi.begin(NAME, PASSWORD);
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());

        while (WiFi.status() != WL_CONNECTED) {
            Serial.print("Network: ");
            Serial.println(NAME);

            if (attempts == 3) {
                Serial.print("Network: ");
                Serial.println(NAME);
                WiFi.begin(NAME, PASSWORD);
                Serial.print("RSSI: ");
                Serial.println(WiFi.RSSI());
            }

            if (attempts <= 10) {
                delay(1000);
                Serial.print(attempts);
                Serial.print(".");

                if (attempts == 10) {
                    String json;
                    const size_t capacity = JSON_OBJECT_SIZE(8);
                    DynamicJsonDocument doc(capacity);
                    doc["wifiSsid"] = NAME;
                    serializeJson(doc, json);

                    preferences.begin("my-app", false);
                    String s = preferences.getString("errors", "yyy");
                    int errors = preferences.getInt("errorCount", 0);
                    if (s == "yyy") {
                        preferences.putString("errors", "Error;Data<br>WiFiError_First_Connection;" + json + "<br>");
                        preferences.putInt("errorCount", errors + 1);
                    } else {
                        String saving = s + "WiFiError_First_Connection;" + json + "<br>";
                        preferences.putString("errors", saving);
                        preferences.putInt("errorCount", errors + 1);
                    }
                    
                    if (errors > 10 && errors < 15) {
                        errorMode = true;
                        activeSoftAP();
                        break;
                    } else if (errors > 15) {
                        preferences.putInt("errorCount", 0);
                        preferences.putString("errors", "");
                    }
                    preferences.end();
                }
            } else {
                if (attempts == 11) {
                    Serial.print("Network: ");
                    Serial.println(ALTERNATIVE_NAME);
                    WiFi.begin(ALTERNATIVE_NAME, ALTERNATIVE_PASSWORD);
                    Serial.print("RSSI: ");
                    Serial.println(WiFi.RSSI());
                }
                
                if (attempts == 20) {
                    Serial.print("Network: ");
                    Serial.println(NAME);
                    WiFi.begin(NAME, PASSWORD);
                    Serial.print("RSSI: ");
                    Serial.println(WiFi.RSSI());
                    attempts = 0;

                    String json;
                    const size_t capacity = JSON_OBJECT_SIZE(8);
                    DynamicJsonDocument doc(capacity);
                    doc["wifiSsid"] = ALTERNATIVE_NAME;
                    serializeJson(doc, json);

                    preferences.begin("my-app", false);
                    String s = preferences.getString("errors", "yyy");
                    int errors = preferences.getInt("errorCount", 0);
                    if (s == "yyy") {
                        preferences.putString("errors", "Error;Data<br>WiFiError_Second_Connection;" + json + "<br>");
                        preferences.putInt("errorCount", errors + 1);
                    } else {
                        String saving = s + "WiFiError_Second_Connection;" + json + "<br>";
                        preferences.putString("errors", saving);
                        preferences.putInt("errorCount", errors + 1);
                    }
                    
                    if (errors > 10 && errors < 15) {
                        errorMode = true;
                        activeSoftAP();
                        break;
                    } else if (errors > 15) {
                        preferences.putInt("errorCount", 0);
                        preferences.putString("errors", "");
                    }
                    preferences.end();
                }
                delay(1000);
                Serial.print(attempts);
                Serial.print(".");
            }
            attempts++;
        }
        attempts = 0;
    }
    if (WiFi.status() == WL_CONNECTED && !timeSynced) {
        syncTime();
    }

    return attempts == 0;
}

void Connect::loadErrorMode() {
    if (millis() - initErrorMode > MAX_ERROR_MODE) {
        preferences.begin("my-app", false);
        preferences.putInt("errorCount", 0);
        preferences.putString("errors", "");
        preferences.end();
        ESP.restart();
    }

    WiFiClient client = server.available();
    if (client) {
        preferences.begin("my-app", false);
        String s = preferences.getString("errors", "yyy");
        int errors = preferences.getInt("errorCount", 0);
        String currentLine = "";
        
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                header += c;
                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");
                        client.println("<style>");
                        client.println("body {");
                        client.println("background-color: #f5ab00;");
                        client.println("}");
                        client.println("</style");

                        client.println("<body><h1> ESP32-ERROR LOG</h1>");
                        client.println("<hr>");
                        client.println("<p>" + s + "</p>");
                        client.println("<div style='text-align: center;'>");
                        client.println("<a href=\"/?buttonRestart\"\"><button style='font-size:150%; background-color:lightblue; color:red;border-radius:100px;'>RESET</button></a>");
                        client.println("</div>");

                        if (header.indexOf("?buttonRestart") > 0) {
                            preferences.begin("my-app", false);
                            preferences.putInt("errorCount", 0);
                            preferences.putString("errors", "");
                            preferences.end();
                            client.println("<h1 style='text-align: center;'>BOARD RESTARTED</h1>");
                            delay(5000);
                            ESP.restart();
                        }

                        client.println("</body></html>");
                        client.stop();
                        delay(1);
                        preferences.end();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        header = "";
        client.stop();  
    }
}

void Connect::getOn(String s) {
    if (millis() - lastSendTime > 12000) {
        Serial.println();
        Serial.print("Sent TURNON: ");
        Serial.println(s);
        
        lastSendTime = millis();

        if (validateStatusWIFI()) {
            StaticJsonDocument<1500> document;
            int wifiRssi = WiFi.RSSI() * -1;
        
            StaticJsonDocument<200> doc;
            String output;
            doc["code"] = s;
            doc["rssi"] = wifiRssi;
            doc["version"] = "12.0.0.0v-point-service-0000030042025.bin";
         
            serializeJson(doc, output);
            
            if (http.begin(API_URL + "/TurnOn")) {
                http.addHeader("Content-Type", "application/json");
                http.setTimeout(15000);
                
                int httpCode = http.POST(output);
                if (httpCode >= 200) {
                    if (httpCode == 200) {
                        lastSendTime = millis();
                        String result = http.getString();
                        DeserializationError error = deserializeJson(document, result);

                        if (!error) {
                            if (document["updateURL"]) {
                                http.end();
                                updateBoard(document["updateURL"]);
                            }
                        }
                    }
                } else {
                    communicationErrors++;
                }
            }
        }
    }
}

void Connect::updateBoard(String url) {
    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, url, "1.0.0.0");
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            break;
        case HTTP_UPDATE_NO_UPDATES:
            break;
        case HTTP_UPDATE_OK:
            break;
    }
}

bool Connect::syncTime() {
    if (WiFi.status() != WL_CONNECTED) {
        timeSynced = false;
        return false;
    }

    if (!timeClientStarted) {
        timeClient.begin();
        timeClientStarted = true;
    }

    const uint8_t maxAttempts = 4;
    for (uint8_t attempt = 0; attempt < maxAttempts; ++attempt) {
        if (timeClient.forceUpdate()) {
            timeSynced = true;
            return true;
        }
        delay(200);
    }

    timeSynced = false;
    return false;
}

uint64_t Connect::getEpochMillis() {
    if (!timeSynced) {
        syncTime();
    }

    unsigned long epochWithOffset = timeClient.getEpochTime();
    if (epochWithOffset == 0) {
        return millis();
    }

    long epochUtc = static_cast<long>(epochWithOffset) - TIME_OFFSET_SECONDS;
    return static_cast<uint64_t>(epochUtc) * 1000ULL;
}

// Métodos MQTT
bool Connect::connectMQTT() {
    if (mqttClient.connected()) {
        return true;
    }
    
    Serial.print("Conectando ao ThingsBoard MQTT...");
    
    // Tenta conectar ao ThingsBoard usando o token como username
    if (mqttClient.connect("ESP32Client", MQTT_TOKEN, nullptr)) {
        Serial.println(" Conectado!");
        return true;
    } else {
        Serial.print(" Falhou, rc=");
        Serial.println(mqttClient.state());
        return false;
    }
}

bool Connect::publishTelemetry(const String& jsonData) {
    return publishTelemetryRaw(jsonData.c_str(), jsonData.length());
}

bool Connect::publishTelemetryRaw(const char* data, size_t length) {
    if (data == nullptr) {
        Serial.println("Falha ao enviar telemetria: payload nulo");
        return false;
    }

    if (!mqttClient.connected()) {
        if (!connectMQTT()) {
            return false;
        }
    }

    if (length == 0) {
        length = strlen(data);
    }

    Serial.print("Publicando telemetria: ");
    Serial.write(reinterpret_cast<const uint8_t*>(data), length);
    Serial.println();

    bool result = mqttClient.publish(MQTT_TOPIC,
                                     reinterpret_cast<const uint8_t*>(data),
                                     length);
    if (result) {
        Serial.println("Telemetria enviada com sucesso!");
    } else {
        Serial.println("Falha ao enviar telemetria");
    }
    return result;
}

bool Connect::isMQTTConnected() {
    return mqttClient.connected();
}

void Connect::loopMQTT() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
}
