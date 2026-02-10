# Sistema de Localização BLE com Geolocalização

Sistema de localização baseado em BLE (Bluetooth Low Energy) para ESP32 com suporte a coordenadas geográficas e integração MQTT/ThingsBoard.

## Descrição

Este projeto implementa um sistema de localização usando ESP32 e BLE para rastrear dispositivos próximos. O sistema é capaz de:

- Detectar dispositivos BLE próximos (beacons, tags, smartphones)
- Calcular distâncias baseadas na força do sinal (RSSI)
- **🆕 Calcular posição geográfica (latitude/longitude) dos beacons detectados**
- **🆕 Enviar dados via MQTT para ThingsBoard**
- Enviar dados para um servidor via HTTP
- Suportar diferentes tipos de dispositivos:
  - Beacons Eddystone TLM (temperatura, bateria)
  - Acelerômetros Minew/Moko
  - Tags BLE personalizadas

## Requisitos

- PlatformIO ou Arduino IDE
- ESP32
- Bibliotecas:
  - BLEDevice
  - WiFi
  - HTTPClient
  - ArduinoJson
  - NTPClient
  - Preferences
  - PubSubClient (para MQTT)

## Configuração

1. Clone o repositório
2. Configure as credenciais WiFi no arquivo [Main/src/config/Config.cpp](Main/src/config/Config.cpp)
3. **🆕 Configure a localização do gateway (lat/long)** no mesmo arquivo
4. Compile e faça upload para o ESP32

### 📍 Configuração de Geolocalização

Para habilitar o cálculo de coordenadas geográficas dos beacons:

1. Edite [Main/src/config/Config.cpp](Main/src/config/Config.cpp):
```cpp
// Localização Fixa do Gateway (AJUSTE PARA SUA LOCALIZAÇÃO REAL)
const double GATEWAY_LATITUDE = -23.223701;   // ← Sua latitude
const double GATEWAY_LONGITUDE = -45.900428;  // ← Sua longitude
const bool ENABLE_GEOLOCATION = true;         // true = habilitar
```

2. Obtenha suas coordenadas:
   - Google Maps: clique direito no local → copie as coordenadas
   - App GPS no celular
   - Dispositivo GPS

**Documentação completa:** [GEOLOCALIZACAO.md](Main/GEOLOCALIZACAO.md)

## Uso

O sistema irá automaticamente:
1. Conectar ao WiFi
2. Sincronizar horário via NTP
3. Conectar ao broker MQTT (ThingsBoard)
4. Iniciar o scanner BLE
5. Coletar dados de dispositivos próximos
6. **🆕 Calcular posição geográfica dos beacons** (se habilitado)
7. Enviar dados via MQTT e HTTP

### Dados enviados (novo formato especificado):

```json
{
  "deviceId": "parque_cerca3",
  "type": "location",
  "data": [
    {
      "objectId": "84:CC:A8:2C:72:30",
      "lat": -23.157145,
      "lng": -45.790568,
      "metadata": {
        "battery": 85,
        "accelerometerX": 0.015,
        "accelerometerY": 0.981,
        "accelerometerZ": 0.134,
        "accuracy": 3.5,
        "signalPower": -65,
        "distance": 2.5
      }
    }
  ],
  "metadata": {
    "espFirmwareVersion": "1.0.2",
    "signalStrength": -58,
    "timestamp": 1737386400000
  }
}
```

**Formato mantém o MAC address original do M2 no campo `objectId`**

## 📚 Documentação

- **[FORMATO_DADOS.md](Main/FORMATO_DADOS.md)** - 🆕 Formato de dados especificado
- **[GEOLOCALIZACAO.md](Main/GEOLOCALIZACAO.md)** - Guia completo de geolocalização
- **[GUIA_THINGSBOARD.md](Main/GUIA_THINGSBOARD.md)** - Integração com ThingsBoard
- **[MQTT_README.md](Main/MQTT_README.md)** - Documentação MQTT
- **[Contrato_Telemetria_v1.md](Main/Contrato_Telemetria_v1.md)** - Especificação dos dados
- **[RESUMO.md](Main/RESUMO.md)** - Resumo rápido
- **[CHECKLIST.md](Main/CHECKLIST.md)** - Lista de verificação

## 🚀 Recursos

- ✅ Detecção BLE automática
- ✅ Cálculo de distância via RSSI
- ✅ **Geolocalização de beacons** (lat/long)
- ✅ Integração MQTT/ThingsBoard
- ✅ Suporte a múltiplos tipos de beacons
- ✅ Heartbeat automático (15s)
- ✅ Sincronização NTP
- ✅ Modo de erro com AP
- ✅ Logs detalhados
