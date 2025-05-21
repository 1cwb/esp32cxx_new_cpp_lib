#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event_base.h"
#include "esp_event.h"
#include <functional>
#include <list>
#include <mutex>

struct stMeventInfo
{
    esp_event_base_t eventBase;
    int32_t eventId;
    void* eventHandlerArg;
    esp_event_handler_instance_t context;

    stMeventInfo(esp_event_base_t base = nullptr, int32_t id = 0, void* arg = nullptr, esp_event_handler_instance_t ctx = nullptr)
        : eventBase(base), eventId(id), eventHandlerArg(arg), context(ctx) {}
};

class MEvent
{
public:
    using MEventHandlerCallback = std::function<void (esp_event_base_t event_base, int32_t event_id, void* event_data)>;
    static MEvent* getInstance()
    {
        static MEvent event;
        return &event;
    }

    MEvent(const MEvent&) = delete;
    MEvent(MEvent&&) = delete;
    MEvent& operator=(const MEvent&) =delete;
    MEvent& operator=(MEvent&&) =delete;

    bool registerEventHandlerInstance(esp_event_base_t eventBase, int32_t eventId, const MEventHandlerCallback& callback);
    bool unregisterEventHandlerInstance(esp_event_base_t eventBase, int32_t eventId);
    bool postEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData, size_t eventDataSize, TickType_t xTicksToWait);
    #if CONFIG_ESP_EVENT_POST_FROM_ISR
    bool postEventIsr(esp_event_base_t eventBase, int32_t eventId, const void* eventData, size_t eventDataSize, BaseType_t* taskUnblocked);
    #endif
private:
    MEvent()
    {
        if(esp_event_loop_create_default() != ESP_OK)
        {
            printf("Error : esp_event_loop_create_default fail\n");
        }
    }
    ~MEvent()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& handler : handlers_)
        {
            esp_event_handler_instance_unregister(handler.eventBase, handler.eventId, handler.context);
            delete static_cast<MEventHandlerCallback*>(handler.eventHandlerArg);
        }
        handlers_.clear();
        esp_event_loop_delete_default();
    }

    static void eventHandlerWrapper(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
    {
        auto callback = static_cast<MEventHandlerCallback*>(event_handler_arg);
        if(callback && (*callback))
        {
            (*callback)(event_base, event_id, event_data);
        }
    }
private:
    std::list<stMeventInfo> handlers_;
    std::mutex mutex_;
};