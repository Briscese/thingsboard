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
#include "headers/User.h"
#include "headers/Distributor.h"
#include "headers/Advertisements.h"
#include "headers/Connect.h"

#define SCAN_INTERVAL 3000
int TIME_MEDIA = 100000;

String placa = "FAB_SJC_CE1_T_0007";

BLEScan* pBLEScan;
Distributor* distributor = nullptr;
Connect* connect = nullptr;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void setup() {
    Serial.begin(115200);

    connect = new Connect(
        "pointservice",
        "HGgs@250",
        "FAB&ITO1",
        "CidadeInteligente",
        "ErrorLogServer123",
        -80,
        1800000
    );

    if(connect->validateStatusWIFI()) {
        connect->getOn(placa);

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
        distributor = new Distributor(User::getAllUsers(), TIME_MEDIA, SCAN_INTERVAL, pBLEScan);
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
            connect->getOn(placa);
        }
    }
}