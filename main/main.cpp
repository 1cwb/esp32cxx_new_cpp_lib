/* pthread/std::thread example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <sstream>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "muart.hpp"
#include "mgpio.hpp"
#include "mled.hpp"
#include "mledstrip.hpp"
#include <cstring>
#include "mevent.hpp"
#include "meventhandle.hpp"
#include "mwifi.hpp"
#include "mespnow.hpp"

#if 0 //smart config
extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    LedStrip ledStrip;
    ledStrip.init(GPIO_NUM_8);
    MWifiStation* pwifiSta = MWifiStation::getInstance();
    pwifiSta->registerWifiEventCb([&](esp_event_base_t eventBase, int32_t eventId, void* eventData){
        if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
            std::thread smartcfg([&]{
                printf("Start smartconfig\r\n");
                ledStrip.setLedColorHSV(HUE_RED, 100, 100);
                pwifiSta->smartConfigStart();
            });
            smartcfg.detach();
        } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
            printf("wifi connect\r\n");
            ledStrip.setLedColorHSV(HUE_YELLOW, 100, 100);
            pwifiSta->connect();

        } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
            ledStrip.setLedColorHSV(HUE_GREEN, 100, 100);
            printf("Got Ip\r\n");
        } else if (eventBase == SC_EVENT && eventId == SC_EVENT_SCAN_DONE) {
            printf("Scan done\r\n");
        } else if (eventBase == SC_EVENT && eventId == SC_EVENT_FOUND_CHANNEL) {
            printf("Found channel\r\n");
        } else if (eventBase == SC_EVENT && eventId == SC_EVENT_GOT_SSID_PSWD) {
            wifi_config_t* wifiConfig = reinterpret_cast<wifi_config_t*>(eventData);
            if(!wifiConfig)
            {
                printf("Error: malloc wifi_config_t fail\n");
                return;
            }
            printf("Got ssid and password\r\n");
            printf("ssid: %s\r\n", (const char*)wifiConfig->sta.ssid);
            printf("password: %s\r\n", (const char*)wifiConfig->sta.password);
            pwifiSta->disconnect();
            pwifiSta->setSsidAndPasswd((const char*)wifiConfig->sta.ssid, (const char*)wifiConfig->sta.password);
            pwifiSta->connect();
        } else if (eventBase == SC_EVENT && eventId == SC_EVENT_SEND_ACK_DONE) {
            printf("connect ok, and stop smartconfig now\r\n");
            pwifiSta->smartConfigStop();
        }
    });
    pwifiSta->init();
    while (1)
    {
       vTaskDelay(pdMS_TO_TICKS(20));
    }
}
#endif

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    uint32_t p = 123;
    LedStrip ledStrip;
    ledStrip.init(GPIO_NUM_8);
    MespNowDataParse* espnowData = new MespNowDataParse;
    MEspNow* pEspNow = MEspNow::getInstance();
    pEspNow->wifiinit();
    pEspNow->espNowInit();
    pEspNow->sendBroadCastToGetAllDevice((const uint8_t*)&p, 4);
    espnowData->setDataParseRecvCb([&](stMespNowEventRecv* recv, bool isbroadCast){
        printf("recv data len: %d, isbroadCast = %d data = %lu\r\n", recv->dataLen, isbroadCast, *(uint32_t*)recv->data);
    });

    while (1)
    {
        p++;
        pEspNow->sendBroadCastToGetAllDevice((const uint8_t*)&p, 4);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}