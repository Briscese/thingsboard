# Sistema de Localização BLE

Sistema de localização baseado em BLE (Bluetooth Low Energy) para ESP32.

## Descrição

Este projeto implementa um sistema de localização usando ESP32 e BLE para rastrear dispositivos próximos. O sistema é capaz de:

- Detectar dispositivos BLE próximos
- Calcular distâncias baseadas na força do sinal (RSSI)
- Enviar dados para um servidor via HTTP
- Suportar diferentes tipos de dispositivos (smartphones, tags, etc.)

## Requisitos

- PlatformIO
- ESP32
- Bibliotecas:
  - BLEDevice
  - WiFi
  - HTTPClient
  - ArduinoJson
  - NTPClient
  - Preferences

## Configuração

1. Clone o repositório
3. Configure as credenciais WiFi no arquivo `src/NetworkManager.cpp`
4. Compile e faça upload para o ESP32

## Uso

O sistema irá automaticamente:
1. Conectar ao WiFi
2. Iniciar o scanner BLE
3. Coletar dados de dispositivos próximos
4. Enviar os dados para o servidor configurado
