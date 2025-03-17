#pragma once
#include "mrmt.hpp"
#include <cstring>

// 基本颜色
constexpr uint16_t HUE_RED = 0;
constexpr uint16_t HUE_ORANGE = 30;
constexpr uint16_t HUE_YELLOW = 60;
constexpr uint16_t HUE_GREEN = 120;
constexpr uint16_t HUE_CYAN = 180;
constexpr uint16_t HUE_BLUE = 240;
constexpr uint16_t HUE_PURPLE = 270;
constexpr uint16_t HUE_MAGENTA = 300;

// 额外的颜色
constexpr uint16_t HUE_ROSE = 330;
constexpr uint16_t HUE_PINK = 350;

// 一些变体
constexpr uint16_t HUE_LIME = 90;
constexpr uint16_t HUE_SPRING_GREEN = 150;
constexpr uint16_t HUE_AZURE = 210;
constexpr uint16_t HUE_VIOLET = 285;

class LedStrip : public RmtEncoderBase
{
public:
    LedStrip()
    {

    }
    ~LedStrip()
    {

    }
    LedStrip(const LedStrip&) = delete;
    LedStrip(LedStrip&&) = delete;
    LedStrip& operator=(const LedStrip& ) = delete;
    LedStrip& operator=(LedStrip&& ) = delete;
    bool init(gpio_num_t gpiox, uint32_t resolution = 10000000);
    void deInit();
    rmt_symbol_word_t* getResetCode();
    bool setLedColorHSV(uint16_t hue, uint8_t saturation, uint8_t value);
    bool off();
private:
    void hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);

private:
    static constexpr int EXAMPLE_LED_NUMBERS = 24;

    MRmtTx tx_;
    uint32_t resolution_;
    rmt_symbol_word_t resetCode_;
    std::mutex mutex_;
};