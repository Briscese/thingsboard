#include "esp_system.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "src/models/User.h"
#include "src/Distributor.h"
#include "src/Advertisements.h"
#include "src/Connect.h"
#include "src/config/Config.h"

BLEScan* pBLEScan;
Distributor* distributor = nullptr;
Connect* connect = nullptr;

void setup() {
  Serial.begin(115200);

    connect = new Connect(
        WIFI_SSID,
        WIFI_PASSWORD,
        WIFI_SSID_ALTERNATIVE,
        WIFI_PASSWORD_ALTERNATIVE,
        SERVER_PASSWORD,
        WIFI_SIGNAL_LIMIT,
        MAX_ERROR_MODE
    );

    if(connect->validateStatusWIFI()) {
        connect->getOn(DEVICE_ID);

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN , ESP_PWR_LVL_P9);

    BLEDevice::init(""); 
        
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(2000);
    pBLEScan->setWindow(1999); 

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    distributor = new Distributor(User::getAllUsers(), pBLEScan);
  }
}

void loop() {
    if (connect->isErrorMode()) {
        connect->loadErrorMode();
    } else {
        if(connect->validateStatusWIFI()) {
            if (distributor != nullptr) {
                distributor->process();
            }
            connect->getOn(DEVICE_ID);
    }
  }
}