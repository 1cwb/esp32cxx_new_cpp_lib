#pragma once
#include "driver/uart.h"
#include <thread>
#include <functional>

class MUart
{
    using UartEventCallBack = std::function<void(void*, size_t)>;
public:
    MUart(uart_port_t uartNum = UART_NUM_1, int32_t baudrate = 921600, uart_word_length_t dataBits = UART_DATA_8_BITS,
        uart_parity_t parity = UART_PARITY_DISABLE, uart_stop_bits_t stopBits = UART_STOP_BITS_1,
        uart_hw_flowcontrol_t flowCtrl = UART_HW_FLOWCTRL_DISABLE);
    ~MUart();
    MUart(const MUart&) = delete;
    MUart(MUart&&) = delete;
    MUart& operator=(const MUart&) =delete;
    MUart& operator=(MUart&&) =delete;
    bool init(int32_t txdPinNum, int32_t rxdPinNum, int32_t rxbufSize = 256, int32_t txbufSize = 256);
    bool deinit();
    size_t sendData(const char *data, size_t size);
    size_t recvData(void *buf, uint32_t length, TickType_t ticks_to_wait);
    bool flushInput();
    bool enablePatternDetBaudIntr(char patternChr, uint8_t chrNum, int chrTout, int postIdle, int preIdle);
    bool disablePatternDetBaudIntr();
    bool getBuffDataLen(size_t *size);
    bool getBuffFreeSize(size_t *size);
    int32_t patternPopPos();
    int32_t patternGetPos();
    void registerEventCallback(const UartEventCallBack& cb);
private:
    uart_port_t uartNum_;
    uart_config_t* config_;
    QueueHandle_t uartQueue_;
    UartEventCallBack* cb_;
};