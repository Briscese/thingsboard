#include "headers/Connect.h"

Connect::Connect(const char* nome, const char* senha, 
                 const char* nome_alternativa, const char* senha_alternativa,
                 const char* server_pwd, int wifi_limit, int max_error_mode)
    : server(80),
      timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000),
      NOME(nome),
      SENHA(senha),
      NOME_ALTERNATIVA(nome_alternativa),
      SENHA_ALTERNATIVA(senha_alternativa),
      server_password(server_pwd),
      WIFI_LIMIT(wifi_limit),
      MAX_ERROR_MODE(max_error_mode),
      errorMode(false),
      sending(false),
      tentativas(0),
      comunicationErrors(0),
      lastSendTime(0),
      initErrorMode(0)
{
    updatePreferences();
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
        WiFi.softAP(s.c_str(), server_password);
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
            Serial.println("Atualizando nome de placa... (RESET)");
            preferences.putString("idPlaca", sendId);
        }
    } else {
        sendId = s;
    }
    preferences.end();
}

bool Connect::validateStatusWIFI() {
    if (WiFi.RSSI() < WIFI_LIMIT) {
        Serial.println("Status Wifi: Desconectado (Potência baixa)");
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
        
        String json;
        const size_t capacity = JSON_OBJECT_SIZE(8);
        DynamicJsonDocument doc(capacity);
        doc["rssiWifi"] = WiFi.RSSI();
        serializeJson(doc, json);

        preferences.begin("my-app", false);
        String s = preferences.getString("erros", "yyy");
        int erros = preferences.getInt("qtdErros", 0);
        if (s == "yyy") {
            preferences.putString("erros", "Erro;Dados<br>ErroWiFi_Out_Range;" + json + "<br>");
            preferences.putInt("qtdErros", erros + 1);
        } else {
            String saving = s + "ErroWiFi_Out_Range;" + json + "<br>";
            preferences.putString("erros", saving);
            preferences.putInt("qtdErros", erros + 1);
        }
        
        if (erros > 10 && erros < 15) {
            errorMode = true;
            activeSoftAP();
        } else if (erros > 15) {
            preferences.putInt("qtdErros", 0);
            preferences.putString("erros", "");
        }
        preferences.end();
        
        WiFi.disconnect();
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Status Wifi: Desconectado");
        WiFi.begin(NOME, SENHA);
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());

        while (WiFi.status() != WL_CONNECTED) {
            Serial.print("Network: ");
            Serial.println(NOME);

            if (tentativas == 3) {
                Serial.print("Network: ");
                Serial.println(NOME);
                WiFi.begin(NOME, SENHA);
                Serial.print("RSSI: ");
                Serial.println(WiFi.RSSI());
            }

            if (tentativas <= 10) {
                delay(1000);
                Serial.print(tentativas);
                Serial.print(".");

                if (tentativas == 10) {
                    String json;
                    const size_t capacity = JSON_OBJECT_SIZE(8);
                    DynamicJsonDocument doc(capacity);
                    doc["wifiSsid"] = NOME;
                    serializeJson(doc, json);

                    preferences.begin("my-app", false);
                    String s = preferences.getString("erros", "yyy");
                    int erros = preferences.getInt("qtdErros", 0);
                    if (s == "yyy") {
                        preferences.putString("erros", "Erro;Dados<br>ErroWiFi_First_Connection;" + json + "<br>");
                        preferences.putInt("qtdErros", erros + 1);
                    } else {
                        String saving = s + "ErroWiFi_First_Connection;" + json + "<br>";
                        preferences.putString("erros", saving);
                        preferences.putInt("qtdErros", erros + 1);
                    }
                    
                    if (erros > 10 && erros < 15) {
                        errorMode = true;
                        activeSoftAP();
                        break;
                    } else if (erros > 15) {
                        preferences.putInt("qtdErros", 0);
                        preferences.putString("erros", "");
                    }
                    preferences.end();
                }
            } else {
                if (tentativas == 11) {
                    Serial.print("Network: ");
                    Serial.println(NOME_ALTERNATIVA);
                    WiFi.begin(NOME_ALTERNATIVA, SENHA_ALTERNATIVA);
                    Serial.print("RSSI: ");
                    Serial.println(WiFi.RSSI());
                }
                
                if (tentativas == 20) {
                    Serial.print("Network: ");
                    Serial.println(NOME);
                    WiFi.begin(NOME, SENHA);
                    Serial.print("RSSI: ");
                    Serial.println(WiFi.RSSI());
                    tentativas = 0;

                    String json;
                    const size_t capacity = JSON_OBJECT_SIZE(8);
                    DynamicJsonDocument doc(capacity);
                    doc["wifiSsid"] = NOME_ALTERNATIVA;
                    serializeJson(doc, json);

                    preferences.begin("my-app", false);
                    String s = preferences.getString("erros", "yyy");
                    int erros = preferences.getInt("qtdErros", 0);
                    if (s == "yyy") {
                        preferences.putString("erros", "Erro;Dados<br>ErroWiFi_Second_Connection;" + json + "<br>");
                        preferences.putInt("qtdErros", erros + 1);
                    } else {
                        String saving = s + "ErroWiFi_Second_Connection;" + json + "<br>";
                        preferences.putString("erros", saving);
                        preferences.putInt("qtdErros", erros + 1);
                    }
                    
                    if (erros > 10 && erros < 15) {
                        errorMode = true;
                        activeSoftAP();
                        break;
                    } else if (erros > 15) {
                        preferences.putInt("qtdErros", 0);
                        preferences.putString("erros", "");
                    }
                    preferences.end();
                }
                delay(1000);
                Serial.print(tentativas);
                Serial.print(".");
            }
            tentativas++;
        }
        tentativas = 0;
    }
    return tentativas == 0;
}

void Connect::loadErrorMode() {
    if (millis() - initErrorMode > MAX_ERROR_MODE) {
        preferences.begin("my-app", false);
        preferences.putInt("qtdErros", 0);
        preferences.putString("erros", "");
        preferences.end();
        ESP.restart();
    }

    WiFiClient client = server.available();
    if (client) {
        preferences.begin("my-app", false);
        String s = preferences.getString("erros", "yyy");
        int erros = preferences.getInt("qtdErros", 0);
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

                        client.println("<body><h1> ESP32-LOG DE ERROS</h1>");
                        client.println("<hr>");
                        client.println("<p>" + s + "</p>");
                        client.println("<div style='text-align: center;'>");
                        client.println("<a href=\"/?buttonRestart\"\"><button style='font-size:150%; background-color:lightblue; color:red;border-radius:100px;'>RESET</button></a>");
                        client.println("</div>");

                        if (header.indexOf("?buttonRestart") > 0) {
                            preferences.begin("my-app", false);
                            preferences.putInt("qtdErros", 0);
                            preferences.putString("erros", "");
                            preferences.end();
                            client.println("<h1 style='text-align: center;'>PLACA REINICIADA</h1>");
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
    if (millis() - lastSendTime > 1200000) { // TIME_ACTIVE
        Serial.println();
        Serial.print("Enviado TURNON: ");
        Serial.println(s);
        
        lastSendTime = millis();

        if (validateStatusWIFI()) {
            StaticJsonDocument<1500> document;
            int wifiRssi = WiFi.RSSI() * -1;
        
            StaticJsonDocument<200> doc;
            String output;
            doc["code"] = s;
            doc["rssi"] = wifiRssi;
            doc["version"] = "11.0.0.0v-point-pqtech-beta0015082024.bin";
         
            serializeJson(doc, output);
            
            if (http.begin(("http://brd-parque-it.pointservice.com.br/api/v1/IOT/TurnOn"))) {
                http.addHeader("Content-Type", "application/json");
                http.setTimeout(15000);
                
                int httpCode = http.POST(output);
                if (httpCode >= 200) {
                    if (httpCode == 200) {
                        lastSendTime = millis();
                        String result = http.getString();
                        DeserializationError error = deserializeJson(document, result);

                        if (!error) {
                            // Processar resposta
                            if (document["updateURL"]) {
                                http.end();
                                updatePlaca(document["updateURL"]);
                            }
                        }
                    }
                } else {
                    comunicationErrors++;
                }
            }
        }
    }
}

void Connect::updatePlaca(String url) {
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
