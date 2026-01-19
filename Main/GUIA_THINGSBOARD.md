# 🚀 Guia de Integração ThingsBoard MQTT

## ✅ O que foi modificado

### 1️⃣ Arquivos Alterados:

#### **Config.h**
- ✅ Adicionadas configurações MQTT do ThingsBoard
- ✅ Host, porta, token e tópico configurados

#### **Connect.h**
- ✅ Adicionada biblioteca `PubSubClient`
- ✅ Novos métodos MQTT públicos
- ✅ Cliente MQTT e WiFi Client adicionados

#### **Connect.cpp**
- ✅ Construtor atualizado com parâmetros MQTT
- ✅ Implementados 4 novos métodos MQTT

#### **Main.ino**
- ✅ Construtor da classe Connect atualizado
- ✅ Adicionado `connect->connectMQTT()` no setup
- ✅ Adicionado `connect->loopMQTT()` no loop

#### **Distributor.h e Distributor.cpp**
- ✅ Novo método `sendDataToThingsBoard()` criado
- ✅ Implementação completa para enviar dados dos usuários BLE

---

## 📦 PASSO 1: Instalar Biblioteca

**Você PRECISA instalar a biblioteca PubSubClient:**

1. Abra o Arduino IDE
2. Vá em: **Sketch** → **Include Library** → **Manage Libraries...**
3. Na busca, digite: `PubSubClient`
4. Instale a biblioteca do autor **Nick O'Leary**

---

## 🔧 PASSO 2: Compilar o Projeto

Após instalar a biblioteca, compile o projeto normalmente. Todas as mudanças já estão implementadas!

---

## 📤 PASSO 3: Como Enviar Dados

### Opção A: Envio Manual Simples

Em qualquer parte do código onde você tem acesso ao objeto `connect`:

```cpp
// Enviar temperatura
String dados = "{\"temperature\":25}";
connect->publishTelemetry(dados);

// Enviar múltiplos valores
String dados = "{\"temperature\":25, \"humidity\":60, \"rssi\":-65}";
connect->publishTelemetry(dados);
```

### Opção B: Usando ArduinoJson (Recomendado)

```cpp
DynamicJsonDocument doc(256);
doc["temperature"] = 25.5;
doc["humidity"] = 60;
doc["deviceId"] = DEVICE_ID;
doc["timestamp"] = millis();

String jsonData;
serializeJson(doc, jsonData);
connect->publishTelemetry(jsonData);
```

### Opção C: Enviar Dados dos Usuários BLE

No seu código `Distributor.cpp`, onde você processa os usuários:

```cpp
// Exemplo: Enviar dados quando um usuário for detectado
for (size_t i = 0; i < users.size(); i++) {
    if (users[i].isLoggedIn()) {
        sendDataToThingsBoard(users[i]);
    }
}
```

Ou no método `process()`, adicione:

```cpp
void Distributor::process() {
    // ... seu código existente ...
    
    // Enviar dados a cada 30 segundos
    static unsigned long lastMqttSend = 0;
    if (millis() - lastMqttSend > 30000) {
        for (size_t i = 0; i < users.size(); i++) {
            if (users[i].isLoggedIn()) {
                sendDataToThingsBoard(users[i]);
                delay(100); // Pequeno delay entre envios
            }
        }
        lastMqttSend = millis();
    }
}
```

---

## 🧪 PASSO 4: Testar a Conexão

Adicione este código no seu `setup()` ou `loop()` para testar:

```cpp
void testMQTT() {
    if (connect->isMQTTConnected()) {
        Serial.println("✓ MQTT Conectado ao ThingsBoard!");
        
        // Enviar dado de teste
        String teste = "{\"status\":\"online\", \"test\":true, \"temperature\":25}";
        if (connect->publishTelemetry(teste)) {
            Serial.println("✓ Teste enviado com sucesso!");
        }
    } else {
        Serial.println("✗ MQTT Desconectado");
        connect->connectMQTT();
    }
}
```

---

## 📊 Visualizar no ThingsBoard

1. Acesse: **http://52.247.226.162:1883** (ou seu endereço ThingsBoard)
2. Faça login na plataforma
3. Vá em: **Devices** → Seu dispositivo
4. Clique na aba: **Latest telemetry**
5. Você verá os dados chegando em tempo real!

---

## 🔍 Monitorar pelo Serial Monitor

Quando você enviar dados, verá mensagens como:

```
Conectando ao ThingsBoard MQTT... Conectado!
Publicando telemetria: {"temperature":25,"humidity":60}
Telemetria enviada com sucesso!
✓ Dados enviados ao ThingsBoard via MQTT
  Payload: {"deviceId":"USER123","rssi":-65,"battery":85}
```

---

## 🎯 Formato dos Dados

O ThingsBoard aceita JSON. Você pode enviar:

```json
{
  "temperature": 25.5,
  "humidity": 60,
  "rssi": -65,
  "battery": 85,
  "deviceId": "FAB_SJC_CE1_T_0007",
  "distance": 2.5,
  "x": 10.5,
  "y": 20.3,
  "z": 5.1,
  "timestamp": 123456789
}
```

**Todos os campos aparecerão como atributos de telemetria no ThingsBoard!**

---

## ⚠️ Importante

1. **Token único**: Cada dispositivo ThingsBoard tem seu próprio token
2. **WiFi primeiro**: Certifique-se de estar conectado ao WiFi antes do MQTT
3. **Loop obrigatório**: Sempre chame `connect->loopMQTT()` no `loop()`
4. **Limite de tamanho**: Mensagens MQTT geralmente têm limite de ~8KB

---

## 🐛 Troubleshooting

### Problema: Não conecta ao MQTT
```cpp
// Verifique se o WiFi está conectado
if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK");
    connect->connectMQTT();
}
```

### Problema: Falha ao publicar
```cpp
// Verifique se está conectado
if (!connect->isMQTTConnected()) {
    Serial.println("Reconectando MQTT...");
    connect->connectMQTT();
}
```

### Problema: Dados não aparecem no ThingsBoard
- Verifique se o token está correto
- Confirme que o formato JSON está válido
- Verifique o Serial Monitor para mensagens de erro

---

## 📝 Exemplo Completo de Uso

```cpp
void loop() {
    // Manter conexão MQTT ativa
    connect->loopMQTT();
    
    // Seu código existente...
    if (connect->validateStatusWIFI()) {
        if (distributor != nullptr) {
            distributor->process();
        }
        
        // Enviar dados de teste a cada 10 segundos
        static unsigned long lastTest = 0;
        if (millis() - lastTest > 10000) {
            DynamicJsonDocument doc(256);
            doc["temperature"] = random(20, 30);
            doc["humidity"] = random(40, 70);
            doc["deviceId"] = DEVICE_ID;
            
            String json;
            serializeJson(doc, json);
            connect->publishTelemetry(json);
            
            lastTest = millis();
        }
    }
}
```

---

## 🎉 Pronto!

Seu dispositivo agora está configurado para enviar dados via MQTT para o ThingsBoard!

**Comando equivalente do mosquitto_pub que você executou:**
```bash
mosquitto_pub -d -q 1 -h 52.247.226.162 -p 1883 -t v1/devices/me/telemetry -u "pAGDT3S1SFG8i4TytyNu" -m "{temperature:25}"
```

**Seu código agora faz exatamente isso, mas automaticamente! 🚀**
