#include "mledstrip.hpp"
bool LedStrip::init(gpio_num_t gpiox, uint32_t resolution)
{
    resolution_ = resolution;
    if(!tx_.init(gpiox, resolution, 0, 4, false))
    {
        return false;
    }
    tx_.registerTxEventCb([](rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata, MRmtTx *user_ctx){
        return true;
    });
    rmt_bytes_encoder_config_t bytes_encoder_config;
    bytes_encoder_config.bit0.level0 = 1;
    bytes_encoder_config.bit0.duration0 = (uint16_t)(0.3 * (float)resolution_ / (float)1000000); // T0H=0.3us
    bytes_encoder_config.bit0.level1 = 0;
    bytes_encoder_config.bit0.duration1 = (uint16_t)(0.9 * (float)resolution_ / (float)1000000); // T0L=0.9us

    bytes_encoder_config.bit1.level0 = 1;
    bytes_encoder_config.bit1.duration0 = (uint16_t)(0.9 * (float)resolution_ / (float)1000000); // T1H=0.9us
    bytes_encoder_config.bit1.level1 = 0;
    bytes_encoder_config.bit1.duration1 = (uint16_t)(0.3 * (float)resolution_ / (float)1000000); // T1L=0.3us
    bytes_encoder_config.flags.msb_first = 1; // WS2812 transfer bit order: G7...G0R7...R0B7...B0

    rmt_copy_encoder_config_t copy_encoder_config = {};
    initBytesEcoder(&bytes_encoder_config);
    initCopyEcoder(&copy_encoder_config);
    uint32_t reset_ticks = resolution_ / 1000000 * 50 / 2; // reset code duration defaults to 50us
    resetCode_.level0 = 0;
    resetCode_.duration0 = reset_ticks;
    resetCode_.level1 = 0;
    resetCode_.duration1 = reset_ticks;

    registerEncoderCb([](rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)->size_t{
        LedStrip *ledStrip = reinterpret_cast<LedStrip*>(encoder);
        rmt_encoder_handle_t bytes_encoder = ledStrip->getBytesEncoder();
        rmt_encoder_handle_t copy_encoder = ledStrip->getCopyEncoder();
        rmt_encode_state_t session_state = RMT_ENCODING_RESET;
        rmt_encode_state_t state = RMT_ENCODING_RESET;
        size_t encoded_symbols = 0;

        do{
            if(ledStrip->getState() == 0) //send rgb code
            {
                encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
                if (session_state & RMT_ENCODING_COMPLETE)
                {
                    ledStrip->setState(1); // switch to next state when current encoding session finished
                }
                if (session_state & RMT_ENCODING_MEM_FULL)
                {
                    state = RMT_ENCODING_MEM_FULL;
                    break;
                }
            }
            if(ledStrip->getState() == 1) // send reset code
            {
                encoded_symbols += copy_encoder->encode(copy_encoder, channel, ledStrip->getResetCode(),
                                                        sizeof(rmt_symbol_word_t), &session_state);
                if (session_state & RMT_ENCODING_COMPLETE)
                {
                    ledStrip->setState(RMT_ENCODING_RESET); // switch to next state when current encoding session finished
                    state = RMT_ENCODING_COMPLETE;
                }
                if (session_state & RMT_ENCODING_MEM_FULL) {
                    state = RMT_ENCODING_MEM_FULL;
                    break;
                }
            }
        }while (0);

        *ret_state = state;
        return encoded_symbols;
    });
    registerResetCb([](rmt_encoder_t *encoder)->esp_err_t{
        LedStrip *ledStrip = reinterpret_cast<LedStrip*>(encoder);
        rmt_encoder_reset(ledStrip->getBytesEncoder());
        rmt_encoder_reset(ledStrip->getCopyEncoder());
        ledStrip->setState(RMT_ENCODING_RESET);
        return ESP_OK;
    });
    registerDelCb([](rmt_encoder_t *encoder)->esp_err_t{
        LedStrip *ledStrip = reinterpret_cast<LedStrip*>(encoder);
        rmt_del_encoder(ledStrip->getBytesEncoder());
        rmt_del_encoder(ledStrip->getCopyEncoder());
        return ESP_OK;
    });
    tx_.enable();
    this->off();
    return true;
}
void LedStrip::deInit()
{
    tx_.deInit();
}
rmt_symbol_word_t* LedStrip::getResetCode()
{
    return &resetCode_;
}

bool LedStrip::setLedColorHSV(uint16_t hue, uint8_t saturation, uint8_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3] = {0};
    rmt_transmit_config_t tx_config;
    tx_config.loop_count = 0;

    // Convert HSV to RGB
    hsv2rgb(hue, saturation, value, &red, &green, &blue);

    // Set all LEDs to the same color
    for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
        led_strip_pixels[i * 3 + 0] = green;  // Green comes first in WS2812
        led_strip_pixels[i * 3 + 1] = red;    // Red is second
        led_strip_pixels[i * 3 + 2] = blue;   // Blue is last
    }

    // Flush RGB values to LEDs
    return tx_.transmit(this, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
}
bool LedStrip::off()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rmt_transmit_config_t tx_config;
    uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3] = {0};
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    tx_config.loop_count = 0;
    return tx_.transmit(this, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
}

void LedStrip::hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}
