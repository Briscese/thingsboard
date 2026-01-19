# Configuração MQTT ThingsBoard

## Mudanças Realizadas

### 1. Config.h
Adicionadas as configurações do ThingsBoard:
```cpp
const char* MQTT_SERVER = "52.247.226.162";
const int MQTT_PORT = 1883;
const char* MQTT_TOKEN = "pAGDT3S1SFG8i4TytyNu";
const char* MQTT_TOPIC = "v1/devices/me/telemetry";
```

### 2. Biblioteca Necessária
Você precisa instalar a biblioteca **PubSubClient** no Arduino IDE:
- Vá em: Sketch → Include Library → Manage Libraries
- Procure por: **PubSubClient**
- Instale a biblioteca

### 3. Como Enviar Dados para o ThingsBoard

No seu código, onde você quiser enviar dados de telemetria, use:

```cpp
// Exemplo 1: Enviar temperatura
String telemetry = "{\"temperature\":25}";
connect->publishTelemetry(telemetry);

// Exemplo 2: Enviar múltiplos dados
String telemetry = "{\"temperature\":25, \"humidity\":60, \"pressure\":1013}";
connect->publishTelemetry(telemetry);

// Exemplo 3: Enviar dados com ArduinoJson
DynamicJsonDocument doc(256);
doc["temperature"] = 25;
doc["humidity"] = 60;
doc["deviceId"] = DEVICE_ID;

String jsonString;
serializeJson(doc, jsonString);
connect->publishTelemetry(jsonString);
```

### 4. Exemplo de Envio no Distributor.cpp

Se você quiser enviar dados BLE via MQTT, adicione no seu `Distributor.cpp`:

```cpp
// No método onde você processa os dados dos usuários
void Distributor::sendDataToThingsBoard(User& user) {
    DynamicJsonDocument doc(512);
    
    doc["deviceId"] = user.getId();
    doc["mac"] = user.getMac();
    doc["rssi"] = user.getAnalog().getValue();
    doc["battery"] = user.getBatteryLevel();
    doc["distance"] = CalculateDistance(user.getAnalog().getValue());
    doc["x"] = user.getX();
    doc["y"] = user.getY();
    doc["z"] = user.getZ();
    doc["timestamp"] = millis();
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Envia via MQTT
    if (connect->publishTelemetry(jsonString)) {
        Serial.println("Dados enviados ao ThingsBoard!");
    }
}
```

### 5. Formato dos Dados ThingsBoard

O ThingsBoard espera dados no formato JSON:
```json
{
  "temperature": 25,
  "humidity": 60,
  "deviceId": "FAB_SJC_CE1_T_0007"
}
```

### 6. Verificar Conexão

Para verificar se está conectado ao MQTT:
```cpp
if (connect->isMQTTConnected()) {
    Serial.println("MQTT Conectado!");
} else {
    Serial.println("MQTT Desconectado");
    connect->connectMQTT();
}
```

### 7. Teste Rápido

No seu `loop()` ou em um timer, você pode adicionar um teste:

```cpp
// Enviar dados de teste a cada 10 segundos
static unsigned long lastTest = 0;
if (millis() - lastTest > 10000) {
    String test = "{\"temperature\":25, \"test\":true}";
    connect->publishTelemetry(test);
    lastTest = millis();
}
```

## Importante

1. **Token de Segurança**: O token `pAGDT3S1SFG8i4TytyNu` é único para cada dispositivo no ThingsBoard
2. **Conexão WiFi**: Certifique-se de que o ESP32 está conectado ao WiFi antes de tentar conectar ao MQTT
3. **Loop MQTT**: O `connect->loopMQTT()` deve ser chamado regularmente no loop() para manter a conexão ativa
4. **Tamanho do Payload**: O ThingsBoard aceita múltiplos atributos em uma única mensagem JSON

## Monitoramento no ThingsBoard

1. Acesse o painel do ThingsBoard
2. Vá em **Devices** → Seu dispositivo
3. Clique na aba **Latest telemetry** para ver os dados recebidos em tempo real
4. Os dados aparecerão conforme você enviar via MQTT
