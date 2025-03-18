#pragma once
#include "mwifi.hpp"
#include "esp_now.h"
#include <mutex>

enum eMespNowEventId
{
    E_ESP_NOW_EVENT_SEND = 0,
    E_ESP_NOW_EVENT_RECV,
    E_ESP_NOW_EVENT_MAX
};

struct stMespNowEventSend
{
    uint8_t macAddr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
};

struct stMespNowEventRecv
{
    uint8_t macAddr[ESP_NOW_ETH_ALEN];
    uint16_t dataLen;
    uint8_t* data;
};

union uMespNowEventInfo
{
    stMespNowEventSend send;
    stMespNowEventRecv recv;
};

struct stMespNowEvent
{
    eMespNowEventId id;
    uMespNowEventInfo info;
};

class MEspNow : public MWifiBase
{
public:
    static MEspNow* getInstance()
    {
        static MEspNow mespnow_;
        return &mespnow_;
    }
    uint32_t getVersion();
    bool wifiinit(wifi_mode_t mode = WIFI_MODE_STA);
    bool espNowInit();
    bool addPeer(const uint8_t *macAddr);
    bool espNowDelPeer(const uint8_t *macAddr);
    bool espNowGetPeerNum(esp_now_peer_num_t *num);
    bool espNowIsPeerExist(const uint8_t *peer_addr);
    bool espNowGetPeer(const uint8_t *peer_addr, esp_now_peer_info_t *peer);
    bool espNowFetchPeer(bool from_head, esp_now_peer_info_t *peer);
    bool espNowSetPeerRate(const uint8_t *peer_addr, esp_now_rate_config_t *config);
    void espNowDeinit();
    bool espSendToPrivateData(const uint8_t *peer_addr, const uint8_t *data, size_t len);
    bool espSendToAllPrivateData(const uint8_t *data, size_t len);
    bool sendBroadCastToGetAllDevice(const uint8_t *data, size_t len);
    static bool isBroadCast(const uint8_t* data, uint8_t len);
private:
    MEspNow() : CONFIG_ESPNOW_PMK("pmk1234567890123")
    {

    }
    ~MEspNow()
    {

    }
    bool createPeer(const uint8_t *peer_addr);
    bool destroyPeer(const uint8_t *peer_addr);
    bool destroyAllPeer();
private:
    const char* CONFIG_ESPNOW_PMK;

    static const uint8_t broadCastMac_[ESP_NOW_ETH_ALEN];
    static const uint32_t ESPNOW_MAXDELAY = 500;
    static const uint8_t ESPNOW_CHANNEL = 1;

    wifi_mode_t mode_;
    std::list<uint8_t *> macList_;
    std::mutex mutex_;
};

class MespNowDataParse : public eventClient
{
    using EventSendCallBack = std::function<void(stMespNowEventSend*,bool)>;
    using EventRecvCallBack = std::function<void(stMespNowEventRecv*,bool)>;
public:
    MespNowDataParse() : sendCb_(new EventSendCallBack),
                         recvCb_(new EventRecvCallBack)
    {
        enableEvent(E_EVENT_ID_ESP_NOW);
        MeventHandler::getINstance()->registerClient(this);
    }
    virtual ~MespNowDataParse()
    {
        if(sendCb_)
        {
            delete sendCb_;
        }
        if(recvCb_)
        {
            delete recvCb_;
        }
        MeventHandler::getINstance()->unregisterClient(this);
    }
    virtual void onEvent(uint32_t eventId, uint8_t* data ,uint32_t dataLen)
    {
        if(!data)
        {
            printf("Error: %s()%d get nullptr\n",__FUNCTION__,__LINE__);
            return;
        }
        if(eventId & E_EVENT_ID_ESP_NOW)
        {
            uint8_t* pdataTemp = nullptr;
            bool bisBroadCast = false;
            stMespNowEvent* evt = reinterpret_cast<stMespNowEvent*>(data);
            stMespNowEvent evtTemp;
            memcpy(&evtTemp, evt, sizeof(stMespNowEvent));
            if(!evt)
            {
                printf("Error: %s()%d get nullptr\n",__FUNCTION__,__LINE__);
                return;
            }
            switch(evt->id)
            {
                case E_ESP_NOW_EVENT_SEND:
                {
                    if(sendCb_ && *sendCb_)
                    {
                        bisBroadCast = MEspNow::isBroadCast(evt->info.send.macAddr, sizeof(evt->info.send.macAddr));
                        (*sendCb_)(&evt->info.send, bisBroadCast);
                    }
                    break;
                }
                case E_ESP_NOW_EVENT_RECV:
                {
                    pdataTemp = evt->info.recv.data;
                    if(!pdataTemp)
                    {
                        printf("Error: %s()%d get nullptr\n",__FUNCTION__,__LINE__);
                        return;
                    }
                    if(recvCb_ && *recvCb_)
                    {
                        bisBroadCast = MEspNow::isBroadCast(evt->info.recv.data, evt->info.recv.dataLen);
                        if(bisBroadCast)
                        {
                            evtTemp.info.recv.data += ESP_NOW_ETH_ALEN;
                            evtTemp.info.recv.dataLen -= ESP_NOW_ETH_ALEN;
                        }
                        (*recvCb_)(&evtTemp.info.recv, bisBroadCast);
                    }
                    break;
                }
                default:
                break;
            }
        }
    }
    void setDataParseSendCb(const EventSendCallBack& sendCb)
    {
        *sendCb_ = sendCb;
    }
    void setDataParseRecvCb(const EventRecvCallBack& recvCb)
    {
        *recvCb_ = recvCb;
    }
private:
    EventSendCallBack* sendCb_;
    EventRecvCallBack* recvCb_;
};
