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
#if 0
extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    uint8_t p[1200] = {65};
    memset(p, 65, sizeof(p));
    LedStrip ledStrip;
    ledStrip.init(GPIO_NUM_8);
    MespNowDataParse* espnowData = new MespNowDataParse;
    MEspNow* pEspNow = MEspNow::getInstance();
    pEspNow->wifiinit();
    pEspNow->espNowInit();
    pEspNow->sendBroadCastToGetAllDevice(pEspNow->getBroadCastMac(), ESP_NOW_ETH_ALEN);
    espnowData->setDataParseRecvCb([&](stMespNowEventRecv* recv, bool isbroadCast){
        if(isbroadCast)
        {
            printf("recv broadCast data, len: %d\n", recv->dataLen);
            if(!pEspNow->espNowIsPeerExist(recv->macAddr))
            {
                pEspNow->addPeer(recv->macAddr);
                pEspNow->sendBroadCastToGetAllDevice(pEspNow->getBroadCastMac(), ESP_NOW_ETH_ALEN);
                ledStrip.setLedColorHSV(HUE_GREEN, 100, 100);
            }
            else
            {
                printf("already add peer, nodify it\n");
                pEspNow->addPeer(recv->macAddr);
                ledStrip.setLedColorHSV(HUE_GREEN, 100, 100);
                uint8_t macaddr[6];
                pEspNow->getMac(pEspNow->getIfx(), macaddr);
                pEspNow->espSendToPrivateData(recv->macAddr, macaddr, sizeof(macaddr));
            }
        }
        else
        {
            if(recv->dataLen == ESP_NOW_ETH_ALEN && memcmp(recv->macAddr, recv->data, ESP_NOW_ETH_ALEN) == 0)
            {
                printf("recv private data, len: %d\n", recv->dataLen);
                pEspNow->addPeer(recv->macAddr);
                ledStrip.setLedColorHSV(HUE_GREEN, 100, 100);
            }
        }
    });
    ledStrip.setLedColorHSV(HUE_RED, 100, 100);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif

extern "C" void app_main(void)
{
    MLed led0(12);
    MLed led1(13);
    led0.OFF();
    led1.OFF();
    MUart uart1;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    MespNowDataParse* espnowData = new MespNowDataParse;
    MEspNow* pEspNow = MEspNow::getInstance();
    pEspNow->wifiinit();
    pEspNow->espNowInit();

    uart1.init(GPIO_NUM_0,GPIO_NUM_1);
    uart1.registerEventCallback([&](void* data, size_t size){
        pEspNow->espSendToAllPrivateData((const uint8_t*)data, size);
    });

    espnowData->setDataParseRecvCb([&](stMespNowEventRecv* recv, bool isbroadCast){
        if(isbroadCast)
        {
            printf("recv broadCast data, len: %d\n", recv->dataLen);
            if(!pEspNow->espNowIsPeerExist(recv->macAddr))
            {
                pEspNow->addPeer(recv->macAddr);
                pEspNow->sendBroadCastToGetAllDevice(pEspNow->getBroadCastMac(), ESP_NOW_ETH_ALEN);
                led0.ON();
            }
            else
            {
                printf("already add peer, nodify it\n");
                pEspNow->addPeer(recv->macAddr);
                led0.ON();
                uint8_t macaddr[6];
                pEspNow->getMac(pEspNow->getIfx(), macaddr);
                pEspNow->espSendToPrivateData(recv->macAddr, macaddr, sizeof(macaddr));
            }
        }
        else
        {
            if(recv->dataLen == ESP_NOW_ETH_ALEN && memcmp(recv->macAddr, recv->data, ESP_NOW_ETH_ALEN) == 0)
            {
                printf("recv private data, len: %d\n", recv->dataLen);
                pEspNow->addPeer(recv->macAddr);
                led0.ON();
            }
            else
            {
                uart1.sendData((char*)recv->data, recv->dataLen);
                led1.toggle();
            }
        }
    });
    pEspNow->sendBroadCastToGetAllDevice(pEspNow->getBroadCastMac(), ESP_NOW_ETH_ALEN);
    while (true) {
        
        vTaskDelay(100);
    }
}
