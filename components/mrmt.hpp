#pragma once
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include <functional>
#include <atomic>

class RmtEncoderBase : protected rmt_encoder_t
{
    using encoderCallback = std::function<size_t(rmt_encoder_t *encoder, rmt_channel_handle_t tx_channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)>;
    using resetCallback = std::function<esp_err_t(rmt_encoder_t *encoder)>;
    using delCallback = std::function<esp_err_t(rmt_encoder_t *encoder)>;
public:
    RmtEncoderBase();
    ~RmtEncoderBase();
    int getState() const;
    void setState(int state);
    bool initBytesEcoder(const rmt_bytes_encoder_config_t *config);
    bool initCopyEcoder( const rmt_copy_encoder_config_t *cpoyConfig);
    bool deInitBytesEcoder();
    bool deInitCopyEcoder();
    bool resetBytesEcoder();
    bool resetCopyEcoder();
    rmt_encoder_handle_t getEncoderHandle();
    rmt_encoder_handle_t getBytesEncoder();
    rmt_encoder_handle_t getCopyEncoder();
    bool registerEncoderCb(const encoderCallback& cb);
    void unregisterEncoderCb();
    bool registerResetCb(const resetCallback& cb);
    void unregisterResetCb();
    bool registerDelCb(const delCallback& cb);
    void unregisterDelCb();
private:
    static size_t encoderCb(rmt_encoder_t *encoder, rmt_channel_handle_t tx_channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
    {
        RmtEncoderBase *encoderBase = reinterpret_cast<RmtEncoderBase*>(encoder);
        if(encoderBase->encoderCb_)
        {
            return encoderBase->encoderCb_(encoder, tx_channel, primary_data, data_size, ret_state);
        }
        return 0;
    }
    static esp_err_t resetCb(rmt_encoder_t *encoder)
    {
        RmtEncoderBase *encoderBase = reinterpret_cast<RmtEncoderBase*>(encoder);
        if(encoderBase->resetCb_)
        {
            return encoderBase->resetCb_(encoder);
        }
        return ESP_FAIL;
    }
    static esp_err_t delCb(rmt_encoder_t *encoder)
    {
        RmtEncoderBase *encoderBase = reinterpret_cast<RmtEncoderBase*>(encoder);
        if(encoderBase->delCb_)
        {
            return encoderBase->delCb_(encoder);
        }
        return ESP_FAIL;
    }
protected:
    rmt_encoder_t *copyEncoder_;
    rmt_encoder_t *bytesEncoder_;
    rmt_bytes_encoder_config_t bytesEncoderConfig_;
    rmt_copy_encoder_config_t copyEncoderConfig_;
    encoderCallback encoderCb_;
    resetCallback resetCb_;
    delCallback delCb_;
    int state_;
};

class RmtBase
{
public:
    RmtBase();
    ~RmtBase();
    RmtBase(const RmtBase&) = delete;
    RmtBase(RmtBase&&) = delete;
    RmtBase& operator=(const RmtBase&) =delete;
    RmtBase& operator=(RmtBase&&) =delete;
    bool init(rmt_channel_handle_t handle);
    bool deInit();
    bool applyCarrier(uint32_t frequencyHz = 38000, float dutyCycle = 0.33, bool alwaysOn = false, bool polarityActiveLow = false);
    bool enable();
    bool disable();
    bool isInit() const {return binit_;}
    rmt_channel_handle_t getHandle() const {return handle_;}
private:
    bool binit_;
    rmt_channel_handle_t handle_;
    rmt_carrier_config_t txCarrierCfg_;
};
class MRmtTx : public RmtBase
{
    using RMTTX_CB = std::function<bool(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata, MRmtTx *user_ctx)>;
public:
    MRmtTx() = default;
    ~MRmtTx() =default;
    MRmtTx(const MRmtTx&) = delete;
    MRmtTx(MRmtTx&&) = delete;
    MRmtTx& operator=(const MRmtTx&) =delete;
    MRmtTx& operator=(MRmtTx&&) =delete;
    bool init(gpio_num_t gpio, uint32_t resolutionHz, int intr_priority, size_t transQueueDepth, bool withDma = true, bool ioLoopBack = false, bool ioOdMode = false, bool invertOut = false, rmt_clock_source_t clkSrc = RMT_CLK_SRC_DEFAULT);
    bool registerTxEventCb(const RMTTX_CB& cb)
    {
        txDoneCb_ = cb;
        orignalCb_.on_trans_done = txDoneCallback;
        esp_err_t err = rmt_tx_register_event_callbacks(getHandle(), &orignalCb_, this);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    void unRegisterTxEventCb()
    {
        txDoneCb_ = nullptr;
    }
    inline void runCallback(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata, MRmtTx *user_ctx)
    {
        if(txDoneCb_)
        {
            txDoneCb_(tx_chan, edata, user_ctx);
        }
    }
    bool transmit(rmt_encoder_t *encoder, const void *payload, size_t payloadBytes, const rmt_transmit_config_t *config);
    bool txWaitAllDone(int timeoutMs);
    void setTransmitBusy(bool busy) {transmitBusy_ = busy;}
    bool isTransmitBusy() const {return transmitBusy_;}
private:
    static bool txDoneCallback(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata, void *user_ctx)
    {
        MRmtTx *tx = static_cast<MRmtTx*>(user_ctx);
        if(tx)
        {
            tx->setTransmitBusy(false);
            tx->runCallback(tx_chan, edata, tx);
        }
        return true;
    }
private:
    std::atomic<bool> transmitBusy_;
    rmt_tx_event_callbacks_t orignalCb_;
    rmt_tx_channel_config_t txChanelCfg_;
    RMTTX_CB txDoneCb_;
};

class MRmtRx : public RmtBase
{
    using RMTRX_CB = std::function<bool(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx)>;
public:
    MRmtRx() = default;
    ~MRmtRx() = default;
    MRmtRx(const MRmtRx&) = delete;
    MRmtRx(MRmtRx&&) = delete;
    MRmtRx& operator=(const MRmtRx&) =delete;
    MRmtRx& operator=(MRmtRx&&) =delete;
    bool init(gpio_num_t gpio, uint32_t resolutionHz, int intr_priority, bool withDma = true, bool ioLoopBack = false, bool allowPd = false, bool invertIn = false, rmt_clock_source_t clkSrc = RMT_CLK_SRC_DEFAULT);
    bool registerRxEventCb(const RMTRX_CB& cb)
    {
        rxDoneCb_ = cb;
        orignalCb_.on_recv_done = rxDoneCallback;
        esp_err_t err = rmt_rx_register_event_callbacks(getHandle(), &orignalCb_, this);
        if(err != ESP_OK)
        {
            printf("Error: %s()%d %s\n",__FUNCTION__,__LINE__,esp_err_to_name(err));
            return false;
        }
        return true;
    }
    void unRegisterRxEventCb()
    {
        rxDoneCb_ = nullptr;
    }
    inline void runCallback(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx)
    {
        if(rxDoneCb_)
        {
            rxDoneCb_(rx_chan, edata, user_ctx);
        }
    }
    bool receive(void *buffer, size_t bufferSize, const rmt_receive_config_t *config);
private:
    static bool rxDoneCallback(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx)
    {
        MRmtRx *rx = static_cast<MRmtRx*>(user_ctx);
        if(rx)
        {
            rx->runCallback(rx_chan, edata, rx);
        }
        return true;
    }
private:
    rmt_rx_event_callbacks_t orignalCb_;
    rmt_rx_channel_config_t rxChanelCfg_;
    RMTRX_CB rxDoneCb_;
};