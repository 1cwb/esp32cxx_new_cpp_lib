#pragma once

#include <iostream>
#include <list>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "esp_interface.h"
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define E_EVENT_ID_INVALID  0X00
#define E_EVENT_ID_KEY      BIT(0)
#define E_EVENT_ID_BUTTON   BIT(1)
#define E_EVENT_ID_ESP_NOW  BIT(2)
#define E_EVENT_ID_ESP_WIFI BIT(3)
#define E_EVENT_ID_ENCODER  BIT(4)

struct stMsgData
{
    uint32_t eventId;
    union {
        uint32_t val;
        uint8_t* data;
    };
    uint32_t dataLen;
    std::function<void(void*)> clean;
};

class eventClient
{
public:
    eventClient() : interestedEvent(E_EVENT_ID_INVALID)
    {

    }
    virtual ~eventClient()
    {
        diableAllEvent();
    }
    void enableEvent(const uint32_t eintersted)
    {
        interestedEvent |= eintersted;
    }
    void disableEvent(const uint32_t eintersted)
    {
        interestedEvent &= ~eintersted;
    }
    void diableAllEvent()
    {
        interestedEvent = E_EVENT_ID_INVALID;
    }
    uint32_t getEvent() const
    {
        return interestedEvent;
    }
    virtual void onEvent(uint32_t eventId, uint8_t* data ,uint32_t dataLen) = 0;
private:
    std::atomic<uint32_t> interestedEvent;
};

class MeventHandler
{
public:
    static MeventHandler* getINstance()
    {
        static MeventHandler eventHandler;
        return &eventHandler;
    }
    QueueHandle_t getQueueHandle() const
    {
        return eventHandleQueue_;
    }
    bool registerClient(eventClient*client);
    void unregisterClient(eventClient*client);
private:
    MeventHandler()
    {
        eventHandleQueue_ = xQueueCreate(EVENT_HANDLE_QUEUE_SIZE, sizeof(stMsgData));
        if (eventHandleQueue_ == nullptr)
        {
            printf("Error: Create mutex fail!\n");
        }
        static std::thread evenhandlerTh_(std::bind(&MeventHandler::onHandler, this));
        evenhandlerTh_.detach();
    }
    ~MeventHandler()
    {
        vQueueDelete(eventHandleQueue_);
    }
    void onHandler();
private:
    static const int EVENT_HANDLE_QUEUE_SIZE = 50;
    QueueHandle_t eventHandleQueue_;
    std::list<eventClient*> eventClientList_;
    std::mutex lock_;
};
