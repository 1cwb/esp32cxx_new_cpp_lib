#include "mevent.hpp"
bool MEvent::registerEventHandlerInstance(esp_event_base_t eventBase,
    int32_t eventId,
    const MEventHandlerCallback& callback)
{
    auto eventHandlerArg = new MEventHandlerCallback(std::move(callback));
    esp_event_handler_instance_t context = nullptr;
    esp_err_t err = esp_event_handler_instance_register(eventBase, eventId, eventHandlerWrapper, eventHandlerArg, &context);
    if(err != ESP_OK)
    {
        delete eventHandlerArg;
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.emplace_back(stMeventInfo(eventBase, eventId, eventHandlerArg, context));
    return true;
}
bool MEvent::unregisterEventHandlerInstance(esp_event_base_t eventBase, int32_t eventId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = handlers_.begin(); it != handlers_.end(); ++it)
    {
        if (it->eventBase == eventBase && it->eventId == eventId)
        {
            esp_err_t err = esp_event_handler_instance_unregister(eventBase, eventId, it->context);
            if(err != ESP_OK)
            {
                printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
                return false;
            }

            delete static_cast<MEventHandlerCallback*>(it->eventHandlerArg);
            handlers_.erase(it);
            return true;
        }
    }
    return false;
}
bool MEvent::postEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData, size_t eventDataSize, TickType_t xTicksToWait)
{
    if(esp_event_post(eventBase, eventId, eventData, eventDataSize, xTicksToWait)!= ESP_OK )
    {
        printf("Error : esp_event_post fail\n");
        return false;
    }
    return true;
}
#if CONFIG_ESP_EVENT_POST_FROM_ISR
bool MEvent::postEventIsr(esp_event_base_t eventBase, int32_t eventId, const void* eventData, size_t eventDataSize, BaseType_t* taskUnblocked)
{
    if(esp_event_isr_post(eventBase, eventId, eventData, eventDataSize, taskUnblocked)  != ESP_OK)
    {
        printf("Error : esp_event_isr_post fail\n");
        return false;
    }
    return true;
}
#endif