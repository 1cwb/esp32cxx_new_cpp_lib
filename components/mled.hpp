#pragma once
#include "mgpio.hpp"
#include <atomic>
class MLed
{
public:
    MLed(int32_t pin, bool highIsOn = false);
    ~MLed();
    MLed(const MLed&) = delete;
    MLed(MLed&&) = delete;
    MLed& operator=(const MLed& ) = delete;
    MLed& operator=(MLed&& ) = delete;
    void ON();
    void OFF();
    int32_t getPinNum() const
    {
        return pin_;
    }
    bool isOn() const
    {
        return on_.load();
    }
    void toggle();
private:
    int32_t pin_;
    std::atomic<bool> on_;
    bool highIsOn_;
    MGpio gpio_;
};