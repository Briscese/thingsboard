# Contrato de Telemetria v1

## Escopo
- Entidades: 1 gateway ESP32 (DEVICE_ID = FAB_SJC_CE1_T_0007) varrendo beacons BLE (MAC address = identificador unico do sensor).
- Destino: ThingsBoard via MQTT publico `v1/devices/me/telemetry` com token por gateway.
- Objetivo: padronizar mensagens heartbeat, telemetry_tlm (Eddystone TLM) e accelerometer (Minew/Moko) sem quebrar dashboards existentes.

## Identificacao e autenticacao
- `gatewayId`: identifica o ESP32. Valor fixo `FAB_SJC_CE1_T_0007` neste firmware.
- `deviceId`: identifica o beacon/sensor descoberto no scan (MAC em formato uppercase com `:`).
- Heartbeat continua enviando apenas `deviceId = gatewayId` para nao quebrar paineis anteriores.
- Token MQTT ativo: `pAGDT3S1SFG8i4TytyNu` (registrado no ThingsBoard para o gateway). QoS 0, retain desabilitado.

## Regras gerais
1. **Timestamp**: campo `ts` em todos os payloads de beacons/telemetria agora e Unix epoch em milissegundos obtido via NTP (`a.st1.ntp.br`, offset -3h). Se o gateway ainda nao sincronizou, o firmware cai para `millis()` e marca o evento em log.
2. **Temperatura**: cada pacote telemetry_tlm envia `temperatureRaw` (int16 big-endian) e `temperatureC`. Conversao = `int8(data[4]) + data[5]/256` graus Celsius.
3. **Acelerometro**: taxa limitada a 1 publish por 500 ms e max 60 mensagens por ciclo de scan, conforme implementado em `Advertisements::processAccelerometer`.
4. **Mensagem type**: campo `messageType` diferencia `heartbeat`, `telemetry_tlm`, `accelerometer` e `aggregated_telemetry` (modo batch opcional).
5. **RSSI**: valores sao enviados em dBm negativos (padrão BLE) para manter compatibilidade com calculo de distancia.

## Topico MQTT
- Topico: `v1/devices/me/telemetry`
- Autenticacao: username = token (`pAGDT3S1SFG8i4TytyNu`), password vazio.
- QoS: 0 (at-most-once). Retain: false.

## Mensagens
### 1. heartbeat (gateway)
- Frequencia: a cada 15 s.
- Campos principais:
  - `deviceId` (string): DEVICE_ID.
  - `uptime` (uint64): tempo de atividade em ms.
  - `wifiRSSI` (int): RSSI WiFi atual.
  - `status` (string): `"online"`.
- Exemplo:
```json
{
  "deviceId": "FAB_SJC_CE1_T_0007",
  "uptime": 1234567,
  "wifiRSSI": -61,
  "status": "online"
}
```

### 2. telemetry_tlm (beacon Eddystone)
- Disparo: sempre que um beacon Eddystone TLM valido e detectado.
- Campos:
  - `messageType`: `"telemetry_tlm"`.
  - `gatewayId`: DEVICE_ID do ESP32 que detectou o beacon.
  - `deviceId` / `mac`: MAC uppercase do beacon (com `:`).
  - `battery` (int): percentual, calculado a partir de `batteryMV`.
  - `batteryMV` (int): milivolts brutos do beacon.
  - `temperatureRaw` (int16): valor como enviado no frame TLM.
  - `temperatureC` (float): graus Celsius.
  - `packetCount` (uint32): contador do beacon.
  - `uptime` (uint32): decimos de segundo do beacon.
  - `rssi` (int): RSSI do scan.
  - `deviceType`: fixo `3` para TLM.
  - `ts` (uint64): Unix epoch ms sincronizado via NTP.
- Exemplo:
```json
{
  "messageType": "telemetry_tlm",
  "gatewayId": "FAB_SJC_CE1_T_0007",
  "deviceId": "BC:57:29:03:7E:33",
  "mac": "BC:57:29:03:7E:33",
  "battery": 82,
  "batteryMV": 2980,
  "temperatureRaw": 6656,
  "temperatureC": 26.0,
  "packetCount": 12045,
  "uptime": 345678,
  "rssi": -72,
  "deviceType": 3,
  "ts": 1737386400000
}
```

### 3. accelerometer (Minew/Moko)
- Disparo: quando um frame correspondendo aos offsets configurados é detectado (Minew UUID 0xFFE1 tipo 0xA1 / Moko UUID 0xFEAB tipo 0x60).
- Limites: 1 publish a cada 500 ms por sensor, max 60 por janela de scan.
- Campos:
  - `messageType`: `"accelerometer"`.
  - `gatewayId`: DEVICE_ID.
  - `deviceId` / `mac`: MAC uppercase do beacon.
  - `battery`: percentual informado pelo frame.
  - `vendor`: `"minew"`, `"moko"` ou `"unknown"`.
  - `sourceUuid`: UUID do service data (0xFFE1 ou 0xFEAB).
  - Componentes XYZ: prefixados conforme vendor (ex.: `x_minew`). Precisao 3 casas.
  - `rssi`: RSSI observado.
  - `deviceType`: `4`.
  - `ts`: Unix epoch ms.
- Exemplo:
```json
{
  "messageType": "accelerometer",
  "gatewayId": "FAB_SJC_CE1_T_0007",
  "deviceId": "BC:57:29:03:7E:33",
  "mac": "BC:57:29:03:7E:33",
  "battery": 68,
  "vendor": "minew",
  "sourceUuid": "0xFFE1",
  "x_minew": -0.015,
  "y_minew": 0.981,
  "z_minew": 0.134,
  "rssi": -65,
  "deviceType": 4,
  "ts": 1737386400500
}
```

### 4. aggregated_telemetry (opcional)
- Origem: `Distributor::sendDataToThingsBoard` quando habilitado para consolidar leituras.
- Campos:
  - `messageType`: `"aggregated_telemetry"`.
  - `gatewayId`, `deviceId`, `mac`, `rssi`, `battery`, `distance`, `x`, `y`, `z`, `timeActivity`, `deviceType`, `loggedIn`, `ts`.
- Uso: dashboards que esperam resumo de presenca por usuario/cartao.

## Taxas e limites
- Heartbeat: 1 msg / 15 s.
- Telemetry TLM: depende da quantidade de beacons, sem limitador adicional.
- Accelerometer: rate limit 500 ms por sensor, 60 mensagens maximas por ciclo de scan.
- Reconexao MQTT automatica via `Connect::loopMQTT()`.

## Consideracoes finais
- `gatewayId` + `deviceId` garantem que integracoes consigam distinguir quem e o gateway e qual sensor originou o dado.
- `ts` usa horario oficial brasileiro (NTP `a.st1.ntp.br`), eliminando a necessidade de corrigir `millis()` do firmware.
- Temperatura agora chega em Celsius pronta para dashboards, mantendo o valor bruto para qualquer correlacao futura.
