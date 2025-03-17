#include "mgpio.hpp"
bool MGpio::isrServerInstalled = false;
MGpio::MGpio(gpio_num_t pin, gpio_mode_t mode, gpio_pullup_t pullUpEn, gpio_pulldown_t pullDownEn, gpio_int_type_t intrType) : pin_(pin)
{
    gpio_reset_pin(pin_);
    gpioCfg_.pin_bit_mask = 1ULL << pin_;
    gpioCfg_.mode = mode;
    gpioCfg_.pull_up_en = pullUpEn;
    gpioCfg_.pull_down_en = pullDownEn;
    gpioCfg_.intr_type = intrType;
    esp_err_t err = gpio_config(&gpioCfg_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
    }
    installIsrService();
    addIsrHandler(gpioIsrHandler, this);
}
MGpio::~MGpio()
{
    if(pin_ == GPIO_NUM_NC)
    {
        return;
    }
    removeIsrHandler();
    gpio_reset_pin(pin_);
    pin_ = GPIO_NUM_NC;
}
bool MGpio::setIntrType(gpio_int_type_t intrType)
{
    esp_err_t err = gpio_set_intr_type(pin_, intrType);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::intrEnable()
{
    esp_err_t err = gpio_intr_enable(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::intrDisable()
{
    esp_err_t err = gpio_intr_disable(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::installIsrService()
{
    if(isrServerInstalled)
    {
        return true;
    }
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    isrServerInstalled = true;
    return true;
}
void MGpio::uninstallIsrService()
{
    if(!isrServerInstalled)
    {
        return;
    }
    gpio_uninstall_isr_service();
    isrServerInstalled = false;
}
bool MGpio::addIsrHandler(gpio_isr_t isr_handler, void *args)
{
    esp_err_t err = gpio_isr_handler_add(pin_, isr_handler, args);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::removeIsrHandler()
{
    esp_err_t err = gpio_isr_handler_remove(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
int32_t MGpio::getLevel()
{
    return gpio_get_level(pin_);
}
bool MGpio::setLevel(uint32_t level)
{
    esp_err_t err = gpio_set_level(pin_, level);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::setDirection(gpio_mode_t mode)
{
    esp_err_t err = gpio_set_direction(pin_, mode);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::setPullMode(gpio_pull_mode_t pullMode)
{
    esp_err_t err = gpio_set_pull_mode(pin_, pullMode);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::wakeupEnable(gpio_int_type_t intrType)
{
    esp_err_t err = gpio_wakeup_enable(pin_, intrType);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

bool MGpio::wakeupDiable()
{
    esp_err_t err = gpio_wakeup_disable(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::pullUpEnable()
{
    esp_err_t err = gpio_pullup_en(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

bool MGpio::pullUpDiable()
{
    esp_err_t err = gpio_pullup_dis(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MGpio::pullDownEnable()
{
    esp_err_t err = gpio_pulldown_en(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}

bool MGpio::pullDownDiable()
{
    esp_err_t err = gpio_pulldown_dis(pin_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}