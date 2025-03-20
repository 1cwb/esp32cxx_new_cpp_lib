#pragma once

#include <functional>
#include <atomic>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_random.h"
#include "esp_smartconfig.h"
#include "meventhandle.hpp"
#include "mevent.hpp"

class MWifiBase : public eventClient
{
    using WifiEventNotifyCb = std::function<void(esp_event_base_t eventBase, int32_t eventId, void* eventData)>;
public:
    MWifiBase();
    virtual ~MWifiBase();
    MWifiBase(const MWifiBase&) = delete;
    MWifiBase(MWifiBase&&) = delete;
    MWifiBase& operator=(const MWifiBase&) =delete;
    MWifiBase& operator=(MWifiBase&&) =delete;
    static bool init();
    static bool deinit();
    static bool start();

    static bool stop();
    static bool bInited() {return (initStatus_ & ESP_WIFI_NETIF_INIT_SUCCESS) && (initStatus_ & ESP_WIFI_INIT_SUCCESS);}
    static bool bStarted() {return initStatus_ & ESP_WIFI_NETIF_STARTED;}
    bool setStorage(wifi_storage_t storage);
    bool setMode(wifi_mode_t mode);
    bool getMode(wifi_mode_t *mode)
    {
        esp_err_t err = esp_wifi_get_mode(mode);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool setChannel(uint8_t primary, wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE)
    {
        esp_err_t err = esp_wifi_set_channel(primary, secondary);
        if(err!= ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool getChannel(uint8_t *primary, wifi_second_chan_t *second)
    {
        esp_err_t err = esp_wifi_get_channel(primary, second);
        if(err!= ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    virtual void onEvent(uint32_t eventId, uint8_t* data ,uint32_t dataLen) override
    {
        stMeventInfo* wifiEvent = reinterpret_cast<stMeventInfo*>(data);
        if(!wifiEvent)
        {
            printf("Error: malloc stMeventInfo fail\n");
            return;
        }
        if(wifiEventNotifyCb_)
        {
            wifiEventNotifyCb_(wifiEvent->eventBase, wifiEvent->eventId, wifiEvent->eventHandlerArg);
        }
    }
    void registerWifiEventCb(const WifiEventNotifyCb& cb)
    {
        wifiEventNotifyCb_ = cb;
    }

    bool getMac(wifi_interface_t ifx, uint8_t mac[6]);
    bool setMac(wifi_interface_t ifx, const uint8_t mac[6]);
    wifi_interface_t getIfx()
    {
        wifi_mode_t mode;
        getMode(&mode);
        return mode == WIFI_MODE_STA ? WIFI_IF_STA : WIFI_IF_AP;
    }
protected:
    static const uint8_t ESP_WIFI_INIT_SUCCESS = BIT(0);
    static const uint8_t ESP_WIFI_NETIF_INIT_SUCCESS = BIT(1);
    static const uint8_t ESP_WIFI_NETIF_STARTED = BIT(2);
    static WifiEventNotifyCb wifiEventNotifyCb_;
private:
    static std::atomic<uint8_t> initStatus_;
    static wifi_init_config_t initCfg_;
    static MEvent::MEventHandlerCallback wifiEventHandleCb_;
    static esp_event_handler_instance_t instanceAndyId_;
    static esp_event_handler_instance_t instanceGotIp_;
    static esp_event_handler_instance_t instanceScEvent_;
};

class MWifiStation : public MWifiBase
{
public:
    static MWifiStation* getInstance();
    bool init(bool onlyStaMode = true);
    bool deinit();
    bool wifiScanStart(const wifi_scan_config_t *config = nullptr, bool block = false);
    bool wifiScanStop();
    bool wifiScanGetApRecords(wifi_ap_record_t** apInfo);
    bool wifiGetScanApNum(uint16_t* apNum);
    bool getMac(uint8_t mac[6])
    {
        return MWifiBase::getMac(WIFI_IF_STA, mac);
    }
    bool setMac(const uint8_t mac[6])
    {
        return MWifiBase::setMac(WIFI_IF_STA, mac);
    }
    bool connect()
    {
        esp_err_t err = esp_wifi_connect();
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool disconnect()
    {
        esp_err_t err = esp_wifi_disconnect();
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool setSsidAndPasswd(const char* ssid, const char* passwd)
    {
        memset(&config_, 0, sizeof(wifi_config_t));
        memcpy(config_.sta.ssid, ssid, strlen(ssid));
        memcpy(config_.sta.password, passwd, strlen(passwd));
        config_.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        config_.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config_);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool smartConfigStart(smartconfig_type_t smartConfigType = SC_TYPE_ESPTOUCH)
    {
        esp_err_t err = ESP_OK;
        smartConfigType_ = smartConfigType;
        err = esp_smartconfig_set_type(smartConfigType_);
        if(err!= ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        err = esp_smartconfig_start(&smartStartConfig_);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    bool smartConfigStop()
    {
        esp_err_t err = esp_smartconfig_stop();
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
private:

    MWifiStation() : staNetif_(nullptr)
    {
        apInfo_ = new wifi_ap_record_t[DEFAULT_SCAN_LIST_SIZE];
        memset(apInfo_, 0, sizeof(wifi_ap_record_t) * DEFAULT_SCAN_LIST_SIZE);
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        memcpy(&smartStartConfig_, &cfg, sizeof(smartconfig_start_config_t));
    }
    ~MWifiStation()
    {
        if(apInfo_)
        {
            delete apInfo_;
            apInfo_ = nullptr;
        }
    }
private:
    static const uint16_t DEFAULT_SCAN_LIST_SIZE = 16;

    esp_netif_t *staNetif_;
    wifi_ap_record_t* apInfo_;
    wifi_config_t config_;
    smartconfig_type_t smartConfigType_;
    smartconfig_start_config_t smartStartConfig_;
};

class MWifiAP : public MWifiBase
{
public:
    static MWifiAP* getInstance();
    bool init(bool onlyApMode = true, const char* ssid = "hellowworld", const char* passwd = "cwb1994228", const uint8_t channel = 1/*1-13*/, const wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK, const bool ssidHidden = false, const uint8_t maxConnection = 4, const wifi_cipher_type_t pairwiseCipher = WIFI_CIPHER_TYPE_TKIP, const bool ftmResponder = false, const bool pmfCapAble = false, const bool pmfRequired = true);
    bool startAp();
    bool stopAp();
    bool deinit();
    bool wifiEventRegister(esp_event_handler_t event_handler);
    bool wifiEventUnregister();
    bool getMac(uint8_t mac[6])
    {
        return MWifiBase::getMac(WIFI_IF_AP, mac);
    }
    bool setMac(const uint8_t mac[6])
    {
        return MWifiBase::setMac(WIFI_IF_AP, mac);
    }
private:
    MWifiAP():apNetif_(nullptr)
    {
        
    }
    ~MWifiAP(){}
private:
    esp_netif_t* apNetif_;
    wifi_config_t apConfig_;
};

class MWifiAPSTA
{
public:
    static MWifiAPSTA* getInstance()
    {
        static MWifiAPSTA wifiapsta;
        return &wifiapsta;
    }
    MWifiStation* getSTA()
    {
        return msta_;
    }
    MWifiAP* getAP()
    {
        return ap_;
    }
    bool init()
    {
        if(!msta_->init(false))
        {
            return false;
        }
        if(!ap_->init(false))
        {
            return false;
        }
        ap_->start();
        return true;
    }

private:
    MWifiAPSTA():msta_(MWifiStation::getInstance()),ap_(MWifiAP::getInstance()){}
    ~MWifiAPSTA(){}
private:
    MWifiStation* msta_;
    MWifiAP* ap_;
};