#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

#define LED_DMA (dmaConfig._2)
#define DMA_CHANNEL_LED 2

#define INVERT

#define PERIOD 23
#define H 15
#define L 9

//#define PERIOD 30
//#define H 10
//#define L 25

#define LED_DATA_BITS 96

volatile uint8 XDATA bitBuffer[LED_DATA_BITS+2] =
{
        255, // Padding

        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,

        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,

        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,

        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,
        H, H, H, H, H, H, H, H,

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
    T3CC0 = PERIOD;       // Set the period
    T3CC1 = 0;       // Set the duty cycle.
    T3CCTL0 = 0b00000100;  // Enable the channel 0 compare, which triggers the DMA at the end of every period.
#ifdef INVERT
    T3CCTL1 = 0b00011100;  // T3CH1: Interrupt disabled, set output on compare up, clear on 0.
#else
    T3CCTL1 = 0b00100100;  // T3CH1: Interrupt disabled, clear output on compare up, set on 0.
#endif
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

void ledStripStartTransfer()
{
    DMAARM |= (1 << DMA_CHANNEL_LED);
}

void ledStripService()
{
    static uint32 lastTime = 0;

    if (getMs() - lastTime >= 30)
    {
        lastTime = getMs();
        ledStripStartTransfer();
    }
}

uint8 volatile XDATA x;

void putchar(char x)
{
    usbComTxSendByte(x);
}

void updateBitBuffer()
{
    static uint8 n = 0;

    if (usbComRxAvailable() && usbComTxAvailable() >= 32)
    {
        uint8 i;
        switch(usbComRxReceiveByte())
        {
        case 'a':
            n++;
            if (n >= LED_DATA_BITS){ n = 0; }
            break;
        case 'd':
            n--;
            if (n == 255){ n = LED_DATA_BITS-1; }
            break;
        default:
            return;
        }
        printf("n=%d\r\n", n);

        for (i = 0; i < LED_DATA_BITS; i++)
        {
            bitBuffer[1+i] = L;
        }
        bitBuffer[1+n] = H;
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
        updateBitBuffer();

        // Spam XDATA with reads and writes to see if there will be a problem.
        __asm mov dptr,#_x __endasm;
        __asm movx @dptr,a __endasm;
        __asm movx @dptr,a __endasm;
        __asm movx @dptr,a __endasm;
        __asm movx @dptr,a __endasm;
        __asm movx a,@dptr __endasm;
        __asm movx a,@dptr __endasm;
        __asm movx a,@dptr __endasm;
        __asm movx a,@dptr __endasm;
    }
}
