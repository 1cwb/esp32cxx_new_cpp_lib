#include "muart.hpp"
#include <cstring>

MUart::MUart(uart_port_t uartNum, int32_t baudrate, uart_word_length_t dataBits, uart_parity_t parity, uart_stop_bits_t stopBits, uart_hw_flowcontrol_t flowCtrl) :
uartNum_(uartNum), config_(nullptr), uartQueue_(nullptr)
{
    config_ = new uart_config_t;
    if(!config_)
    {
        printf("error: new uart_config_t fail\n");
        return;
    }
    cb_ = new UartEventCallBack;
    memset(config_, 0, sizeof(uart_config_t));
    config_->baud_rate = baudrate;
    config_->data_bits = dataBits;
    config_->parity    = parity;
    config_->stop_bits = stopBits;
    config_->flow_ctrl = flowCtrl;
    config_->source_clk = UART_SCLK_DEFAULT;
}
MUart::~MUart()
{
    if(config_)
    {
        delete config_;
        config_ = nullptr;
    }
    if(cb_)
    {
        delete cb_;
        cb_ = nullptr;
    }
}

bool MUart::init(int32_t txdPinNum, int32_t rxdPinNum, int32_t rxbufSize, int32_t txbufSize)
{
    if(uart_is_driver_installed(uartNum_))
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,"already inited");
        return false;
    }
    esp_err_t err = uart_driver_install(uartNum_, rxbufSize, txbufSize, 20, &uartQueue_, 0);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    err = uart_param_config(uartNum_, config_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    err = uart_set_pin(uartNum_, txdPinNum, rxdPinNum, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    #define PATTERN_CHR_NUM    (3)
    std::thread uartthread([this](){
        uart_event_t event;
        size_t buffered_size;
        uint8_t* dtmp = (uint8_t*) malloc(128);
        while (true)
        {
            //Waiting for UART event.
            if (xQueueReceive(uartQueue_, (void *)&event, (TickType_t)portMAX_DELAY))
            {
                bzero(dtmp, 128);
                switch (event.type)
                {
                    //Event of UART receiving data
                    /*We'd better handler data event fast, there would be much more data events than
                    other types of events. If we take too much time on data event, the queue might
                    be full.*/
                    case UART_DATA:
                        recvData(dtmp, event.size, portMAX_DELAY);
                        if(cb_ && *cb_)
                        {
                            (*cb_)(dtmp, event.size);
                        }
                        break;
                    //Event of HW FIFO overflow detected
                    case UART_FIFO_OVF:
                        printf("hw fifo overflow\r\n");
                        // If fifo overflow happened, you should consider adding flow control for your application.
                        // The ISR has already reset the rx FIFO,
                        // As an example, we directly flush the rx buffer here in order to read more data.
                        flushInput();
                        xQueueReset(uartQueue_);
                        break;
                    //Event of UART ring buffer full
                    case UART_BUFFER_FULL:
                        printf("ring buffer full\r\n");
                        // If buffer full happened, you should consider increasing your buffer size
                        // As an example, we directly flush the rx buffer here in order to read more data.
                        flushInput();
                        xQueueReset(uartQueue_);
                        break;
                    //Event of UART RX break detected
                    case UART_BREAK:
                        printf("uart rx break\r\n");
                        break;
                    //Event of UART parity check error
                    case UART_PARITY_ERR:
                        printf("uart parity error\r\n");
                        break;
                    //Event of UART frame error
                    case UART_FRAME_ERR:
                        printf("uart frame error\r\n");
                        break;
                    //UART_PATTERN_DET
                    case UART_PATTERN_DET:
                    {
                        getBuffDataLen(&buffered_size);
                        int pos = patternPopPos();
                        printf("[UART PATTERN DETECTED] pos: %d, buffered size: %d\r\n", pos, buffered_size);
                        if (pos == -1) {
                            // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                            // record the position. We should set a larger queue size.
                            // As an example, we directly flush the rx buffer here.
                            flushInput();
                        } else {
                            recvData(dtmp, pos, 100 / portTICK_PERIOD_MS);
                            uint8_t pat[PATTERN_CHR_NUM + 1];
                            memset(pat, 0, sizeof(pat));
                            recvData(pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                            printf("read data: %s\r\n", dtmp);
                            printf("read pat : %s\r\n", pat);
                        }
                    }
                    break;
                    default:
                        printf("uart event type: %d\r\n", event.type);
                        break;
                }
            }
        }
        free(dtmp);
        dtmp = NULL;
    });
    uartthread.detach();
    return true;
}
bool MUart::deinit()
{
    esp_err_t err = uart_driver_delete(uartNum_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
size_t MUart::sendData(const char *data, size_t size)
{
    return uart_write_bytes(uartNum_, data, size);
}
size_t MUart::recvData(void *buf, uint32_t length, TickType_t ticks_to_wait)
{
    return uart_read_bytes(uartNum_, buf, length, ticks_to_wait);
}
bool MUart::flushInput()
{
    esp_err_t err = uart_flush_input(uartNum_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MUart::enablePatternDetBaudIntr(char patternChr, uint8_t chrNum, int chrTout, int postIdle, int preIdle)
{
    esp_err_t err = uart_enable_pattern_det_baud_intr(uartNum_, patternChr, chrNum, chrTout, postIdle, preIdle);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MUart::disablePatternDetBaudIntr()
{
    esp_err_t err = uart_disable_pattern_det_intr(uartNum_);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MUart::getBuffDataLen(size_t *size)
{
    esp_err_t err = uart_get_buffered_data_len(uartNum_, size);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
bool MUart::getBuffFreeSize(size_t *size)
{
    esp_err_t err = uart_get_tx_buffer_free_size(uartNum_, size);
    if(err != ESP_OK)
    {
        printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
        return false;
    }
    return true;
}
int32_t MUart::patternPopPos()
{
    return uart_pattern_pop_pos(uartNum_);
}
int32_t MUart::patternGetPos()
{
    return uart_pattern_get_pos(uartNum_);
}
void MUart::registerEventCallback(const UartEventCallBack& cb)
{
    if(cb_)
    {
        *cb_ = cb;
    }
}