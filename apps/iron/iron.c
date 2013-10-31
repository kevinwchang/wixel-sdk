#include <wixel.h>

#include <usb.h>
#include <usb_com.h>

#include <radio_com.h>
#include <radio_link.h>

#define SOMO_CLK  04
#define SOMO_DATA 05

#define SOUND_POWER_UP    0x0000
#define SOUND_FIRE        0x0001
#define SOUND_POWER_DOWN  0x0002

#define SHIFTBRITE_LATCH    P1_7
#define SHIFTBRITE_DATA     P1_6
#define SHIFTBRITE_CLOCK    P1_5
#define SHIFTBRITE_DISABLE  P1_4

#define HELMET_BUTTON 0
#define CHARGE_BUTTON 1
#define FIRE_BUTTON   2

void somoInit()
{
    setDigitalOutput(SOMO_CLK, HIGH);
    setDigitalOutput(SOMO_DATA, LOW);
}

void somoCmd(uint16 cmd)
{
    int8 b;
    
    setDigitalOutput(SOMO_CLK, LOW);
    delayMs(2);
    
    for (b = 15; b >= 0; b--)
    {
        setDigitalOutput(SOMO_CLK, LOW);
        setDigitalOutput(SOMO_DATA, (cmd >> b) & 1);
        delayMicroseconds(100);
        setDigitalOutput(SOMO_CLK, HIGH);
        delayMicroseconds(100);

    }
    delayMs(2);
}

void shiftbriteInit()
{
    SHIFTBRITE_DISABLE = 1; // disable shiftbrites until a valid color is sent
    SHIFTBRITE_CLOCK = 0; // clock low
    SHIFTBRITE_LATCH = 0; // prevent unintended latching
    P1DIR |= (1<<4); // P1_4 = Disable !Enable
    P1DIR |= (1<<5); // P1_5 = Clock
    P1DIR |= (1<<6); // P1_6 = Data
    P1DIR |= (1<<7); // P1_7 = Latch
}

void toggleLatch()
{
    SHIFTBRITE_LATCH = 1;
    delayMicroseconds(1);
    SHIFTBRITE_LATCH = 0;
    delayMicroseconds(1);
    SHIFTBRITE_DISABLE = 0; // enable shiftbrites
}

void sendBit(BIT value)
{
    SHIFTBRITE_DATA = value;
    delayMicroseconds(1);
    SHIFTBRITE_CLOCK = 1;
    delayMicroseconds(1);
    SHIFTBRITE_CLOCK = 0;
    delayMicroseconds(1);
}

void sendRGB(uint16 r, uint16 g, uint16 b)
{
    uint16 mask = 512;
    sendBit(0);
    sendBit(0);
    while(mask)
    {
        sendBit((mask & b) ? 1 : 0);
        mask >>= 1;
    }
    mask = 512;
    while(mask)
    {
        sendBit((mask & r) ? 1 : 0);
        mask >>= 1;
    }
    mask = 512;
    while(mask)
    {
        sendBit((mask & g) ? 1 : 0);
        mask >>= 1;
    }
}

static uint16 last_light;

void setLight(uint16 brightness)
{
    last_light = brightness;
    sendRGB(0, 0, brightness);
    toggleLatch();
}

void main()
{
    enum repulsorState {IDLE, POWERING_UP, POWERED_UP, FIRING, WAITING_FOR_RELEASE, POWERING_DOWN} state = IDLE;
    uint16 timing_start_ms = 0, elapsed;

    BIT helmet_button_pressed = 0;
    
    systemInit();
    usbInit();
    radioComInit();
    
    shiftbriteInit();
    somoInit();
    
    while (1)
    {
        usbShowStatusWithGreenLed();
        boardService();
        radioComTxService();
        usbComService();
        
        if (!helmet_button_pressed && !isPinHigh(HELMET_BUTTON))
        {
            delayMs(10);
            if (!isPinHigh(HELMET_BUTTON) && radioComTxAvailable())
            {
                helmet_button_pressed = 1;
                radioComTxSendByte((uint8)'!');
            }
        }
        else if (helmet_button_pressed && isPinHigh(HELMET_BUTTON))
        {
            delayMs(10);
            if (isPinHigh(HELMET_BUTTON))
                helmet_button_pressed = 0;
        }
        
        switch (state)
        {
            case IDLE:
                if (!isPinHigh(CHARGE_BUTTON))
                {
                    delayMs(10);
                    if (!isPinHigh(CHARGE_BUTTON))
                    {
                        somoCmd(SOUND_POWER_UP);
                        timing_start_ms = getMs();
                        state = POWERING_UP;
                    }
                }
                break;
                
            case POWERING_UP:
                elapsed = getMs() - timing_start_ms;
                if (elapsed >= 1500)
                    state = POWERED_UP;
                else if (elapsed >= 256 && elapsed < 1280)
                    setLight((elapsed - 256) >> 2);
                break;
                
            case POWERED_UP:
                if (!isPinHigh(FIRE_BUTTON))
                {
                    delayMs(10);
                    if (!isPinHigh(FIRE_BUTTON))
                    {
                        somoCmd(SOUND_FIRE);
                        setLight(0);
                        timing_start_ms = getMs();
                        state = FIRING;
                    }
                }
                else if (isPinHigh(CHARGE_BUTTON))
                {
                    delayMs(10);
                    if (isPinHigh(CHARGE_BUTTON))
                    {
                        somoCmd(SOUND_POWER_DOWN);
                        timing_start_ms = getMs();
                        state = POWERING_DOWN;
                    }
                }
                break;
                
            case FIRING:
                elapsed = getMs() - timing_start_ms;
                if (elapsed > 1500)
                    state = WAITING_FOR_RELEASE;
                else if (elapsed > 150)
                {
                    if (last_light != 0)
                        setLight(0);
                }
                else if (elapsed > 50)
                {
                    if (last_light != 0x3FF)
                        setLight(0x3FF);
                }
                break;
                
            case WAITING_FOR_RELEASE:
                if (isPinHigh(FIRE_BUTTON) && isPinHigh(CHARGE_BUTTON))
                {
                    delayMs(10);
                    if (isPinHigh(FIRE_BUTTON) && isPinHigh(CHARGE_BUTTON))
                        state = IDLE;
                }
                break;
                
            case POWERING_DOWN:
                elapsed = getMs() - timing_start_ms;
                if (elapsed >= 1500)
                    state = IDLE;
                else if (elapsed < 1024)
                    setLight(255 - (elapsed >> 2));
                break;
            
            default:
                state = IDLE;
        }
    }
}



