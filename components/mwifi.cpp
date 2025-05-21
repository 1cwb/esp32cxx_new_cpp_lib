#include "mwifi.hpp"
#include <cstring>

std::atomic<uint8_t> MWifiBase::initStatus_(0x00);
wifi_init_config_t MWifiBase::initCfg_;
MEvent::MEventHandlerCallback MWifiBase::wifiEventHandleCb_ = nullptr;
esp_event_handler_instance_t MWifiBase::instanceAndyId_ = nullptr;
esp_event_handler_instance_t MWifiBase::instanceGotIp_ = nullptr;
esp_event_handler_instance_t MWifiBase::instanceScEvent_ = nullptr;
MWifiBase::WifiEventNotifyCb MWifiBase::wifiEventNotifyCb_ = nullptr;
MWifiBase::MWifiBase()
{
    enableEvent(E_EVENT_ID_ESP_WIFI);
    MeventHandler::getINstance()->registerClient(this);
    init();
}
MWifiBase::~MWifiBase()
{
    stop();
    deinit();
}
bool MWifiBase::init()
{
    if(bInited())
    {
        return true;
    }
    esp_err_t err = esp_netif_init();
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    initStatus_ |= ESP_WIFI_NETIF_INIT_SUCCESS;
    wifi_init_config_t tempCfg = WIFI_INIT_CONFIG_DEFAULT();
    initCfg_ = tempCfg;
    memcpy(&initCfg_, &tempCfg, sizeof(initCfg_));
    err = esp_wifi_init(&initCfg_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    initStatus_ |= ESP_WIFI_INIT_SUCCESS;

    wifiEventHandleCb_ = [](esp_event_base_t event_base, int32_t event_id, void* event_data)
    {
        stMsgData msg;
        stMeventInfo* wifiEvent = new stMeventInfo;//reinterpret_cast<stMeventInfo*>(malloc(sizeof(stMeventInfo)));
        if(!wifiEvent)
        {
            printf("Error: malloc stMeventInfo fail\n");
            return;
        }
        wifiEvent->eventBase = event_base;
        wifiEvent->eventId = event_id;
        if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            wifiEvent->eventHandlerArg = malloc(sizeof(ip_event_got_ip_t));
            if(!wifiEvent->eventHandlerArg)
            {
                delete(wifiEvent);
                wifiEvent = nullptr;
                printf("Error: malloc ip_event_got_ip_t fail\n");
                return;
            }
            memcpy(wifiEvent->eventHandlerArg, event_data, sizeof(ip_event_got_ip_t));
        }
        else if(event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
        {
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
            wifi_config_t* wifiConfig = reinterpret_cast<wifi_config_t*>(malloc(sizeof(wifi_config_t)));
            if(!wifiConfig)
            {
                delete(wifiEvent);
                wifiEvent = nullptr;
                printf("Error: malloc smartconfig_event_got_ssid_pswd_t fail\n");
                return;
            }
            bzero(wifiConfig, sizeof(wifi_config_t));
            memcpy(wifiConfig->sta.ssid, evt->ssid, sizeof(wifiConfig->sta.ssid));
            memcpy(wifiConfig->sta.password, evt->password, sizeof(wifiConfig->sta.password));
            wifiConfig->sta.bssid_set = evt->bssid_set;
            if (wifiConfig->sta.bssid_set == true)
            {
                memcpy(wifiConfig->sta.bssid, evt->bssid, sizeof(wifiConfig->sta.bssid));
            }

            wifiEvent->eventHandlerArg =  reinterpret_cast<wifi_config_t*>(wifiConfig);
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifiEvent->eventHandlerArg = malloc(sizeof(wifi_event_ap_staconnected_t));
            if(!wifiEvent->eventHandlerArg)
            {
                delete(wifiEvent);
                wifiEvent = nullptr;
                printf("Error: malloc wifi_event_ap_staconnected_t fail\n");
                return;
            }
            memcpy(wifiEvent->eventHandlerArg, event_data, sizeof(wifi_event_ap_staconnected_t));
        }
        else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifiEvent->eventHandlerArg = malloc(sizeof(wifi_event_ap_stadisconnected_t));
            if(!wifiEvent->eventHandlerArg)
            {
                delete(wifiEvent);
                wifiEvent = nullptr;
                printf("Error: malloc wifi_event_ap_stadisconnected_t fail\n");
                return;
            }
            memcpy(wifiEvent->eventHandlerArg, event_data, sizeof(wifi_event_ap_stadisconnected_t));
        }
        msg.eventId = E_EVENT_ID_ESP_WIFI;
        msg.data = reinterpret_cast<uint8_t*>(wifiEvent);
        msg.dataLen = sizeof(stMeventInfo);
        msg.clean = [](void* data){
            if(data)
            {
                stMeventInfo* p = reinterpret_cast<stMeventInfo*>(data);
                if(p->eventHandlerArg)
                {
                    delete(reinterpret_cast<wifi_config_t*>(p->eventHandlerArg));
                }
                delete(p);
            }
        };
        if(xQueueSend(MeventHandler::getINstance()->getQueueHandle(), &msg, 20) != pdTRUE)
        {
            printf("Error: %s()%d send queue fail!\n",__FUNCTION__,__LINE__);
        }
    };
    MEvent::getInstance()->registerEventHandlerInstance(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        wifiEventHandleCb_);
    MEvent::getInstance()->registerEventHandlerInstance(IP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        wifiEventHandleCb_);
    MEvent::getInstance()->registerEventHandlerInstance(SC_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        wifiEventHandleCb_);
    return true;
}
bool MWifiBase::deinit()
{
    esp_err_t err = ESP_OK;
    MEvent::getInstance()->unregisterEventHandlerInstance(WIFI_EVENT, ESP_EVENT_ANY_ID);
    MEvent::getInstance()->unregisterEventHandlerInstance(IP_EVENT, ESP_EVENT_ANY_ID);
    MEvent::getInstance()->unregisterEventHandlerInstance(SC_EVENT, ESP_EVENT_ANY_ID);
    if(initStatus_ & ESP_WIFI_NETIF_INIT_SUCCESS)
    {
        err = esp_netif_deinit();
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        }
        initStatus_ &= ~ ESP_WIFI_NETIF_INIT_SUCCESS;
    }
    if(initStatus_ & ESP_WIFI_INIT_SUCCESS)
    {
        err = esp_wifi_deinit();
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        }
        initStatus_ &= ~ ESP_WIFI_INIT_SUCCESS;
    }
    if(initStatus_ != 0)
    {
        initStatus_ = 0;
        return false;
    }
    return true;
}
bool MWifiBase::start()
{
    if(initStatus_ & ESP_WIFI_NETIF_STARTED)
    {
        return true;
    }
    if(!bInited())
    {
        return false;
    }
    esp_err_t err = esp_wifi_start();
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    initStatus_ |= ESP_WIFI_NETIF_STARTED;
    return true;
}

bool MWifiBase::stop()
{
    if(!(initStatus_ & ESP_WIFI_NETIF_STARTED))
    {
        printf("wifi already stoped!\n");
        return true;
    }
    esp_err_t err = esp_wifi_stop();
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    initStatus_ &= ~ESP_WIFI_NETIF_STARTED;
    return true;
}
bool MWifiBase::setStorage(wifi_storage_t storage)
{
    esp_err_t err = esp_wifi_set_storage(storage);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MWifiBase::setMode(wifi_mode_t mode)
{
    esp_err_t err = esp_wifi_set_mode(mode);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

bool MWifiBase::getMac(wifi_interface_t ifx, uint8_t mac[6])
{
    esp_err_t err = esp_wifi_get_mac(ifx, mac);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MWifiBase::setMac(wifi_interface_t ifx, const uint8_t mac[6])
{
    esp_err_t err = esp_wifi_set_mac(ifx, mac);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

//////////////////////////////////////////////STA/////////////////////////////////////////////////
MWifiStation* MWifiStation::getInstance()
{
    static MWifiStation wifista;
    return &wifista;
}
bool MWifiStation::init(bool onlyStaMode)
{
    if(!bInited())
    {
        return false;
    }
    if(!wifiEventNotifyCb_)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,"Please register wifi event callback befor init\n");
        return false;
    }
    staNetif_ = esp_netif_create_default_wifi_sta();
    if(!staNetif_)
    {
        printf("Error %s()%d fail\n",__FUNCTION__,__LINE__);
        return false;
    }
    wifi_mode_t mode = onlyStaMode ? WIFI_MODE_STA : WIFI_MODE_APSTA;
    if(!setMode(mode))
    {
        return false;
    }
    if(onlyStaMode)
    {
        start(); //STA_AP MODE start NOT HERE
    }
    return true;
}
bool MWifiStation::deinit()
{
    if(staNetif_)
    {
        esp_netif_destroy_default_wifi(staNetif_);
    }
    return true;
}
bool MWifiStation::wifiScanStart(const wifi_scan_config_t *config, bool block)
{
    esp_err_t err = esp_wifi_scan_start(config, block);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MWifiStation::wifiScanStop()
{
    esp_err_t err = esp_wifi_scan_stop();
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MWifiStation::wifiScanGetApRecords(wifi_ap_record_t** apInfo)
{
    uint16_t scanNumLimit = DEFAULT_SCAN_LIST_SIZE;
    esp_err_t err = esp_wifi_scan_get_ap_records(&scanNumLimit, apInfo_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        *apInfo = nullptr;
        return false;
    }
    *apInfo = apInfo_;
    return true;
}
bool MWifiStation::wifiGetScanApNum(uint16_t* apNum)
{
    esp_err_t err = esp_wifi_scan_get_ap_num(apNum);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    if(*apNum > DEFAULT_SCAN_LIST_SIZE)
    {
        *apNum = DEFAULT_SCAN_LIST_SIZE;
    }
    return true;
}
//////////////////////////////////////////////////AP////////////////////////////////////////////
MWifiAP* MWifiAP::getInstance()
{
    static MWifiAP wifiap;
    return &wifiap;
}
bool MWifiAP::init(bool onlyApMode, const char* ssid, const char* passwd, const uint8_t channel/*1-13*/, const wifi_auth_mode_t authmode, const bool ssidHidden, const uint8_t maxConnection, const wifi_cipher_type_t pairwiseCipher, const bool ftmResponder, const bool pmfCapAble, const bool pmfRequired)
{
    if(!bInited())
    {
        return false;
    }
    if(!wifiEventNotifyCb_)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,"Please register wifi event callback befor init\n");
        return false;
    }
    esp_err_t err = ESP_FAIL;
    apNetif_ = esp_netif_create_default_wifi_ap();
    if(!apNetif_)
    {
        printf("Error %s()%d fail\n",__FUNCTION__,__LINE__);
        return false;
    }
    memcpy(apConfig_.ap.ssid, ssid, strlen(ssid));
    memcpy(apConfig_.ap.password, passwd, strlen(passwd));
    apConfig_.ap.ssid_len = strlen(ssid);
    apConfig_.ap.channel = channel;
    apConfig_.ap.ssid_hidden = ssidHidden ? 1 : 0;
    apConfig_.ap.max_connection = maxConnection;
    apConfig_.ap.beacon_interval = 100;
    apConfig_.ap.pairwise_cipher = pairwiseCipher;
    apConfig_.ap.ftm_responder = ftmResponder;
    apConfig_.ap.pmf_cfg.capable = pmfCapAble;
    apConfig_.ap.pmf_cfg.required = pmfRequired;
    if (strlen(passwd) == 0)
    {
        apConfig_.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        apConfig_.ap.authmode = authmode;
    }
    wifi_mode_t mode = onlyApMode ? WIFI_MODE_AP : WIFI_MODE_APSTA;
    err = esp_wifi_set_mode(mode);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &apConfig_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MWifiAP::startAp()
{
    return start();
}
bool MWifiAP::stopAp()
{
    return stop();
}
bool MWifiAP::deinit()
{
    if(apNetif_)
    {
        esp_netif_destroy_default_wifi(apNetif_);
    }
    return true;
}