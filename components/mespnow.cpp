#include "mespnow.hpp"

const uint8_t MEspNow::broadCastMac_[ESP_NOW_ETH_ALEN] = {0xff,0xff,0xff,0xff,0xff,0xff};

uint32_t MEspNow::getVersion()
{
    uint32_t version = 0;
    esp_err_t err = esp_now_get_version(&version);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return 0;
    }
    return version;
}
bool MEspNow::wifiinit(wifi_mode_t mode)
{
    wifi_mode_t currentMode = WIFI_MODE_NULL;
    if(!setStorage(WIFI_STORAGE_RAM))
    {
        return false;
    }
    if(getMode(&currentMode))
    {
        if(currentMode == WIFI_MODE_NULL)
        {
            currentMode = mode;
            if(!setMode(mode))
            {
                return false;
            }
        }
    }
    mode_  = currentMode;
    if(!MWifiBase::start())
    {
        return false;
    }
    if(setChannel(ESPNOW_CHANNEL))
    {
        return false; 
    }
#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    esp_err_t err = esp_wifi_set_protocol(mode_ == WIFI_MODE_STA ? WIFI_IF_STA : WIFI_IF_AP, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
#endif
    return true;
}
bool MEspNow::espNowInit()
{
    esp_err_t err = esp_now_init();
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    //typedef void (*esp_now_send_cb_t)(const uint8_t *mac_addr, esp_now_send_status_t status);
    err = esp_now_register_send_cb([](const uint8_t *macAddr, esp_now_send_status_t status){
        if(macAddr == nullptr)
        {
            printf("Error: %s()%d send arg error!\n",__FUNCTION__,__LINE__);
            return;
        }
        stMsgData msg;
        stMespNowEvent* evt = static_cast<stMespNowEvent*>(malloc(sizeof(stMespNowEvent)));
        if(!evt)
        {
            printf("Error %s()%d : malloc Fail\n",__FUNCTION__,__LINE__);
            return;
        }

        msg.eventId = E_EVENT_ID_ESP_NOW;
        msg.data = reinterpret_cast<uint8_t*>(evt);
        msg.dataLen = sizeof(stMespNowEvent);
        msg.clean = [](void* data){
            if(data)
            {
                free(data);
            }
        };
        stMespNowEventSend* psend = &evt->info.send;

        evt->id = E_ESP_NOW_EVENT_SEND;
        memcpy(psend->macAddr, macAddr, ESP_NOW_ETH_ALEN);
        psend->status = status;
        if(xQueueSend(MeventHandler::getINstance()->getQueueHandle(), &msg, ESPNOW_MAXDELAY) != pdTRUE)
        {
            printf("Error: %s()%d send queue fail!\n",__FUNCTION__,__LINE__);
        }
    });
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    //typedef void (*esp_now_recv_cb_t)(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    err = esp_now_register_recv_cb([](const esp_now_recv_info_t * espNowInfo, const uint8_t *data, int data_len){
        if(espNowInfo->src_addr == nullptr || data == nullptr || data_len <= 0)
        {
            printf("Error: %s()%d recv arg error!\n",__FUNCTION__,__LINE__);
            return;
        }
        stMsgData msg;
        stMespNowEvent* evt = static_cast<stMespNowEvent*>(malloc(sizeof(stMespNowEvent)));
        if(!evt)
        {
            printf("Error %s()%d : malloc Fail\n",__FUNCTION__,__LINE__);
            return;
        }

        msg.eventId = E_EVENT_ID_ESP_NOW;
        msg.data = reinterpret_cast<uint8_t*>(evt);
        msg.dataLen = sizeof(stMespNowEvent);
        msg.clean = [](void* data){
            if(data)
            {
                stMespNowEvent* evt = static_cast<stMespNowEvent*>(data);
                if(evt->info.recv.data)
                {
                    free(evt->info.recv.data);
                }
                free(evt);
            }
        };
        
        stMespNowEventRecv* precv = &evt->info.recv;

        evt->id = E_ESP_NOW_EVENT_RECV;
        memcpy(precv->macAddr, espNowInfo->src_addr, ESP_NOW_ETH_ALEN);
        precv->dataLen = data_len;
        precv->data = static_cast<uint8_t*>(malloc(data_len));
        if(precv->data == nullptr)
        {
            printf("Error: %s()%d malloc fail\n",__FUNCTION__,__LINE__);
            return;
        }
        memcpy(precv->data, data, data_len);
        if(xQueueSend(MeventHandler::getINstance()->getQueueHandle(), &msg, ESPNOW_MAXDELAY) != pdTRUE)
        {
            printf("Error: %s()%d send queue fail!\n",__FUNCTION__,__LINE__);
        }
    });
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    //ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    //ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
    err = esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    if(!addPeer(broadCastMac_))
    {
        return false;
    }
    return true;
}

bool MEspNow::addPeer(const uint8_t *macAddr)
{
    esp_err_t err = ESP_OK;
    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = static_cast<esp_now_peer_info_t*>(malloc(sizeof(esp_now_peer_info_t)));
    if(!peer)
    {
        printf("tony %s()%d Malloc Fail\n",__FUNCTION__,__LINE__);
        return false;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = ESPNOW_CHANNEL;
    peer->ifidx =  (mode_ == WIFI_MODE_STA ? WIFI_IF_STA : WIFI_IF_AP);
    peer->encrypt = false;
    memcpy(peer->peer_addr, macAddr, ESP_NOW_ETH_ALEN);
    if(espNowIsPeerExist(macAddr))
    {
        err = esp_now_mod_peer(peer);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
            free(peer);
            return false;
        }
        if(!destroyPeer(macAddr))
        {
            printf("Error: %s()%d destroyPeer fail",__FUNCTION__,__LINE__);
        }
    }
    else
    {
        err = esp_now_add_peer(peer);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
            free(peer);
            return false;
        }
    }
    free(peer);
    if(!createPeer(macAddr))
    {
        printf("Error: %s()%d createPeer fail",__FUNCTION__,__LINE__);
    }
    return true;
}

bool MEspNow::espNowDelPeer(const uint8_t *macAddr)
{
    esp_err_t err = esp_now_del_peer(macAddr);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    if(!destroyPeer(macAddr))
    {
        printf("Error: reove peer from peerlist Fail\n");
    }
    return true;
}
bool MEspNow::espNowGetPeerNum(esp_now_peer_num_t *num)
{
    esp_err_t err = esp_now_get_peer_num(num);
    if(err != ESP_OK || num->total_num != macList_.size())
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        printf("Error: %s()%d num = %u, macList_.size() = %u",__FUNCTION__,__LINE__,num->total_num,macList_.size());
        return false;
    }
    return true;
}
bool MEspNow::espNowIsPeerExist(const uint8_t *peer_addr)
{
    return esp_now_is_peer_exist(peer_addr);
}

bool MEspNow::espNowGetPeer(const uint8_t *peer_addr, esp_now_peer_info_t *peer)
{
    esp_err_t err = esp_now_get_peer(peer_addr, peer);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MEspNow::espNowFetchPeer(bool from_head, esp_now_peer_info_t *peer)
{
    esp_err_t err = esp_now_fetch_peer(from_head, peer);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MEspNow::espNowSetPeerRate(const uint8_t *peer_addr, esp_now_rate_config_t *config)
{
    esp_err_t err = esp_now_set_peer_rate_config(peer_addr, config);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

void MEspNow::espNowDeinit()
{
    esp_now_deinit();
    destroyAllPeer();
}
bool MEspNow::espSendToPrivateData(const uint8_t *peer_addr, const uint8_t *data, size_t len)
{
    esp_err_t err = esp_now_send(peer_addr, data, len);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MEspNow::espSendToAllPrivateData(const uint8_t *data, size_t len)
{
    for(const auto& it: macList_)
    {
        if(it)
        {
            if(isBroadCast(it, ESP_NOW_ETH_ALEN))
            {
                continue;
            }
            if(!espSendToPrivateData(it, data, len))
            {
                return false;
            }
        }
    }
    return true;
}
bool MEspNow::sendBroadCastToGetAllDevice(const uint8_t *data, size_t len)
{
    if(len > 250 - ESP_NOW_ETH_ALEN)
    {
        len = 250 - ESP_NOW_ETH_ALEN;
    }
    size_t newLen = len + ESP_NOW_ETH_ALEN;
    uint8_t* buffer = static_cast<uint8_t*>(malloc(newLen));
    if(buffer == nullptr)
    {
        printf("Error: malloc fail\n");
        return false;
    }
    memset(buffer, 0, newLen);
    memcpy(buffer, broadCastMac_, ESP_NOW_ETH_ALEN);
    if(data != nullptr && len > 0)
    {
        memcpy(&buffer[ESP_NOW_ETH_ALEN] , data, len);
    }
    if(!espSendToPrivateData(broadCastMac_, buffer, newLen))
    {
        printf("Error : send Broad Cast Info Fail\n");
        free(buffer);
        return false;
    }
    free(buffer);
    return true;
}
bool MEspNow::isBroadCast(const uint8_t* data, uint8_t len)
{
    if(len < ESP_NOW_ETH_ALEN)
    {
        return false;
    }
    if(data)
    {
        return (memcmp(broadCastMac_, data, ESP_NOW_ETH_ALEN) == 0);
    }
    return false;
}

bool MEspNow::createPeer(const uint8_t *peer_addr)
{
    uint8_t *peerAddr = static_cast<uint8_t*>(malloc(ESP_NOW_ETH_ALEN));
    if(!peerAddr)
    {
        printf("Error : %s()%d Malloc fail !\n",__FUNCTION__,__LINE__);
        return false;
    }
    memcpy(peerAddr, peer_addr, ESP_NOW_ETH_ALEN);
    std::lock_guard<std::mutex> lock(mutex_);
    macList_.emplace_back(peerAddr);
    return true;
}
bool MEspNow::destroyPeer(const uint8_t *peer_addr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto it = macList_.begin(); it != macList_.end();)
    {
        if(*it)
        {
            //printf("tony (*it)->peer_addr = "MACSTR", peerAddr = "MACSTR"\n",MAC2STR((*it)),MAC2STR(peer_addr));
            if(memcmp(*it, peer_addr, ESP_NOW_ETH_ALEN) == 0)
            {
                free(*it);
                it = macList_.erase(it);
                return true;
            }
            else
            {
                it ++;
            }
        }
        else
        {
            it = macList_.erase(it);
        }
    }
    return false;
}
bool MEspNow::destroyAllPeer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto it = macList_.begin(); it != macList_.end();)
    {
        if(*it)
        {
            //printf("tony (*it)->peer_addr = "MACSTR", peerAddr = "MACSTR"\n",MAC2STR((*it)),MAC2STR(peer_addr));
            free(*it);
            it = macList_.erase(it);
        }
        else
        {
            it = macList_.erase(it);
        }
    }
    return true;
}