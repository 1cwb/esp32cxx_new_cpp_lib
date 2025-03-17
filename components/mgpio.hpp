#pragma once
#include "driver/gpio.h"
#include <functional>

class MGpio
{
    using GPIO_ISR_CB = std::function<void(void*)>;
public:
    MGpio(gpio_num_t pin = GPIO_NUM_NC, gpio_mode_t mode = GPIO_MODE_OUTPUT, gpio_pullup_t pullUpEn = GPIO_PULLUP_DISABLE, gpio_pulldown_t pullDownEn = GPIO_PULLDOWN_DISABLE, gpio_int_type_t intrType = GPIO_INTR_DISABLE);
    ~MGpio();
    MGpio(const MGpio&) = delete;
    MGpio(MGpio&&) = delete;
    MGpio& operator=(const MGpio& ) = delete;
    MGpio& operator=(MGpio&& ) = delete;
    bool setIntrType(gpio_int_type_t intrType = GPIO_INTR_NEGEDGE);
    bool intrEnable();
    bool intrDisable();
    int32_t getPin() const {return (int32_t) pin_;}
    bool installIsrService();
    void uninstallIsrService();
    int32_t getLevel();
    bool setLevel(uint32_t level);
    bool setDirection(gpio_mode_t mode);
    bool setPullMode(gpio_pull_mode_t pullMode);
    bool wakeupEnable(gpio_int_type_t intrType);
    bool wakeupDiable();
    bool pullUpEnable();
    bool pullUpDiable();
    bool pullDownEnable();
    bool pullDownDiable();
    void registerIsrCallback(const GPIO_ISR_CB& cb) {isrCb_ = cb;}
    void unregisterIsrCallback() {isrCb_ = nullptr;}
    void runIsrCallback() {if(isrCb_) isrCb_(this);}
private:
    bool addIsrHandler(gpio_isr_t isr_handler, void *args);
    bool removeIsrHandler();
    static void gpioIsrHandler(void* args)
    {
        MGpio* gpio = (MGpio*)args;
        gpio->runIsrCallback();
    }
    gpio_num_t pin_;
    gpio_config_t gpioCfg_;
    GPIO_ISR_CB isrCb_;
    static bool isrServerInstalled;
};