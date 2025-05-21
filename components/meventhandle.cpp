#include "meventhandle.hpp"
bool MeventHandler::registerClient(eventClient*client)
{
    if(!client)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(lock_);
    for(auto& c : eventClientList_)
    {
        if(client == c)
        {
            printf("Warning : The client has been registered in eventHandler\n");
            return true;
        }
    }
    eventClientList_.emplace_back(client);
    return true;
}
void MeventHandler::unregisterClient(eventClient*client)
{
    if(client)
    {
        client->diableAllEvent();
        std::lock_guard<std::mutex> lock(lock_);
        for(auto it = eventClientList_.begin(); it != eventClientList_.end();)
        {
            if((*it) == client)
            {
                it = eventClientList_.erase(it);
            }
            else
            {
                it ++;
            }
        }
    }
}
void MeventHandler::onHandler()
{
    stMsgData msg;
    printf("eventHandler Run...\n");
    while(true)
    {
        if(xQueueReceive(eventHandleQueue_, &msg, portMAX_DELAY) == pdTRUE)
        {
            std::lock_guard<std::mutex> lock(lock_);
            for(auto& c : eventClientList_)
            {
                if(c->getEvent() & msg.eventId)
                {
                    c->onEvent(c->getEvent() & msg.eventId, msg.data, msg.dataLen);
                }
            }
            if(msg.clean)
            {
                msg.clean(msg.data);
            }
        }
    }
}
