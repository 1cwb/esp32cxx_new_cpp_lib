#include "mled.hpp"

MLed::MLed(int32_t pin, bool highIsOn) 
: pin_(pin),
  on_(false),
  highIsOn_(highIsOn),
  gpio_(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE)
{
    OFF();
}
MLed::~MLed()
{

}
void MLed::ON()
{
    if(highIsOn_)
    {
        gpio_.setLevel(1);
    }
    else
    {
        gpio_.setLevel(0);
    }
    on_ = true;
}
void MLed::OFF()
{
    if(highIsOn_)
    {
        gpio_.setLevel(0);
    }
    else
    {
        gpio_.setLevel(1);
    }
    on_ = false;
}

void MLed::toggle()
{
    if(on_)
    {
        OFF();
    }
    else
    {
        ON();
    }
}