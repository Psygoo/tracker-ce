#include "mtPadsBacklight.h"
#include "Platform.h"

#if !TRACKER_CE_ENABLE_WR2
#define sendLed(n,state,level) leds->setLed(n,state,level)
#endif
cMtPadsBacklight padsBacklight;

void cMtPadsBacklight::init(uint16_t blink, Leds* leds)
{
    blinkConstrain = blink;
    blinkTimer = 0;
    cMtPadsBacklight::leds = leds;
}

void cMtPadsBacklight::update()
{
    if (blinkCounter)
    {
        if (blinkTimer > blinkConstrain)
        {
            blinkTimer = 0;

            if (toggler == 1)
                toggler = 0;
            else if (toggler == 0)
                toggler = 1;

            for (uint8_t i = 0; i < 48; i++)
            {
                if (blinkLayer[i])
                    setFrontLayer(toggler, blinkLayer[i], i);
            }
        }
    }
}

void cMtPadsBacklight::setBackLayer(uint8_t state, uint8_t gamma_pwm, uint8_t n)
{
    if (state)
    {
        sendLed(n, state, (gamma_pwm > frontLayer[n] ? gamma_pwm : frontLayer[n]));
        backLayer[n] = gamma_pwm;
    }
    else
    {
        if (frontLayer[n] != 0)
            sendLed(n, 1, frontLayer[n]);
        else
            sendLed(n, state, gamma_pwm);

        backLayer[n] = 0;
    }
}

void cMtPadsBacklight::setFrontLayer(uint8_t state, uint8_t gamma_pwm, uint8_t n)
{
    if (state)
    {
        sendLed(n, state, (gamma_pwm > backLayer[n] ? gamma_pwm : backLayer[n]));
        frontLayer[n] = gamma_pwm;
    }
    else
    {
        if (backLayer[n] != 0)
            sendLed(n, 1, backLayer[n]);
        else
            sendLed(n, state, gamma_pwm);

        frontLayer[n] = 0;
    }
}

void cMtPadsBacklight::startBlink(uint8_t gamma_pwm, uint8_t n)
{
    blinkLayer[n] = gamma_pwm;
    blinkCounter++;
}

void cMtPadsBacklight::stopBlink(uint8_t n)
{
    blinkLayer[n] = 0;
    setFrontLayer(0, blinkLayer[n], n);
    blinkCounter--;
}

void cMtPadsBacklight::clearGlobalPads(uint8_t front, uint8_t back, uint8_t blink)
{
    if (front)
    {
        for (uint8_t i = 0; i < 48; i++)
        {
            setFrontLayer(0, 0, i);
            frontLayer[i] = 0;
        }
    }

    if (back)
    {
        for (uint8_t i = 0; i < 48; i++)
        {
            setBackLayer(0, 0, i);
            backLayer[i] = 0;
        }
    }
    if (blink)
    {
        for (uint8_t i = 0; i < 48; i++)
        {
            stopBlink(i);
        }
    }
}

#if TRACKER_CE_ENABLE_WR2
void cMtPadsBacklight::sendLed(uint8_t n,uint8_t state,uint8_t level){
    if(!wr2OverlayOn)leds->setLed(n,state,level);
}
void cMtPadsBacklight::setWr2Overlay(const uint8_t*levels){
    for(uint8_t i=0;i<48;++i){
        if(!wr2OverlayOn||wr2Levels[i]!=levels[i])leds->setLed(i,levels[i]!=0,levels[i]);
        wr2Levels[i]=levels[i];
    }
    wr2OverlayOn=true;
}
void cMtPadsBacklight::clearWr2Overlay(){
    if(!wr2OverlayOn)return;
    wr2OverlayOn=false;
    for(uint8_t i=0;i<48;++i){auto level=frontLayer[i]>backLayer[i]?frontLayer[i]:backLayer[i];leds->setLed(i,level!=0,level);}
}
#endif
