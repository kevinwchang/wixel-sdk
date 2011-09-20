#include <wixel.h>
#include <usb.h>
#include <usb_com.h>

#define LED_DMA (dmaConfig._2)
#define DMA_CHANNEL_LED 2

#define H 15
#define L 8

volatile uint8 XDATA bitBuffer[96+1] =
{
        H, H, H, H, H, H, H, H,  // Red 1
        L, L, L, L, L, L, L, L,  // Green 1
        H, H, H, H, L, L, L, L,  // Blue

        L, H, L, H, L, H, L, H,
        H, L, H, L, H, L, H, L,
        H, H, L, L, H, H, L, L,

        H, H, H, H, L, L, L, L,
        H, L, H, L, H, L, H, L,
        L, L, L, L, L, H, L, L,

        H, L, L, L, L, L, L, L,
        H, L, L, L, L, L, L, L,
        H, L, L, L, L, L, L, L,

        0,  // Reset
};

// Experimentally, we found that duty cycles of 10 or lower are bad.
// It must be because the DMA takes some time.

void ledStripInit()
{
    P1_4 = 0;
    P1DIR |= (1<<4);
    P1SEL |= (1<<4);  // Assign P1_4 to a peripheral function instead of GPIO.

    // Set Timer 3 modulo mode (period is set by T3CC0)
    // Set Timer 3 Channel 1 (P1_4) to output compare mode.
    T3CC0 = 23;       // Set the period
    T3CC1 = 0;       // Set the duty cycle.
    T3CCTL0 = 0b00000100;  // Enable the channel 0 compare, which triggers the DMA at the end of every period.
    T3CCTL1 = 0b00100100;  // T3CH1: Timer disabled, clear output on compare up, set on 0.
    T3CTL = 0b00010010;  // Start the timer with Prescaler 1:1, modulo mode (counts from 0 to T3CC0).

    LED_DMA.DC6 = 19; // WORDSIZE = 0, TMODE = 0, TRIG = 19
    LED_DMA.SRCADDRH = (unsigned int)bitBuffer >> 8;
    LED_DMA.SRCADDRL = (unsigned int)bitBuffer;
    LED_DMA.DESTADDRH = XDATA_SFR_ADDRESS(T3CC1) >> 8;
    LED_DMA.DESTADDRL = XDATA_SFR_ADDRESS(T3CC1);
    LED_DMA.LENL = sizeof(bitBuffer);
    LED_DMA.VLEN_LENH = 0;
    LED_DMA.DC6 = 0b00000111; // WORSIZE = 0, TMODE = single, TRIG = 7 (Timer 3 compare ch 0)
    LED_DMA.DC7 = 0b01000010; // SRCINC = 1, DESTINC = 0, IRQMASK = 0, M8 = 0, PRIORITY = 2 (High)

    // We found that I priority of 1 (equal to the CPU) also works.

}

void ledStripService()
{
    static uint32 lastTime = 0;

    if (getMs() - lastTime >= 30)
    {
        lastTime = getMs();
        DMAARM |= (1 << DMA_CHANNEL_LED);
    }
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
        LED_YELLOW(getMs() >> 9 & 1);

        boardService();
        ledStripService();


        // Spam XDATA with reads and writes to see if there will be a problem.
        __asm movx  @dptr,a __endasm;
        __asm movx  @dptr,a __endasm;
        __asm movx  @dptr,a __endasm;
        __asm movx  @dptr,a __endasm;
        __asm movx  a,@dptr __endasm;
        __asm movx  a,@dptr __endasm;
        __asm movx  a,@dptr __endasm;
        __asm movx  a,@dptr __endasm;
    }
}
