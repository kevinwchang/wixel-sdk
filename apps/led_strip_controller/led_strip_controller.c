#include <wixel.h>
#include <usb.h>
#include <usb_com.h>

#define LED_DMA (dmaConfig._2)
#define DMA_CHANNEL_LED 2

uint8 XDATA bitBuffer[96];

// Experimentally, we found that duty cycles of 10 or lower are bad.
// It must be because the DMA takes some time.

void ledStripInit()
{
    uint8 i;

    P1_4 = 0;
    P1DIR |= (1<<4);
    P1SEL |= (1<<4);  // Assign P1_4 to a peripheral function instead of GPIO.

    // Set Timer 3 modulo mode (period is set by T3CC0)
    // Set Timer 3 Channel 1 (P1_4) to output compare mode.
    T3CC0 = 23;       // Set the period
    T3CC1 = 11;       // Set the duty cycle.
    T3CCTL0 = 0b00000100;  // Enable the channel 0 compare, which triggers the DMA.
    T3CCTL1 = 0b00100100;  // T3CH1: Timer disabled, clear output on compare up, set on 0.
    T3CTL = 0b00010010;  // Start the timer with Prescaler 1:1, modulo mode (counts from 0 to T3CC0).

    for(i=0; i < sizeof(bitBuffer); i += 4)
    {
        bitBuffer[i+0] = 8;
        bitBuffer[i+1] = 15;
        bitBuffer[i+2] = 8;
        bitBuffer[i+3] = 15;
    }

    LED_DMA.DC6 = 19; // WORDSIZE = 0, TMODE = 0, TRIG = 19
    LED_DMA.SRCADDRH = (unsigned int)bitBuffer >> 8;
    LED_DMA.SRCADDRL = (unsigned int)bitBuffer;
    LED_DMA.DESTADDRH = XDATA_SFR_ADDRESS(T3CC1) >> 8;
    LED_DMA.DESTADDRL = XDATA_SFR_ADDRESS(T3CC1);
    LED_DMA.LENL = sizeof(bitBuffer);
    LED_DMA.VLEN_LENH = 0;
    LED_DMA.DC6 = 0b01000111; // WORSIZE = 0, TMODE = repeated single, TRIG = 7 (Timer 3 compare ch 0)
    LED_DMA.DC7 = 0b01000010; // SRCINC = 1, DESTINC = 0, IRQMASK = 0, M8 = 0, PRIORITY = 2 (High)

    DMAARM |= (1 << DMA_CHANNEL_LED);
}

void main()
{
    systemInit();
    usbInit();
    ledStripInit();

    while(1)
    {
        usbComService();
        usbShowStatusWithGreenLed();
        boardService();
    }
}
