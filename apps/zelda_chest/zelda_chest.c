/** example_blink_led app:
This app blinks the red LED.

For a precompiled version of this app and a tutorial on how to load this app
onto your Wixel, see the Pololu Wixel User's Guide:
http://www.pololu.com/docs/0J46
*/

#include <wixel.h>
#include <usb.h>
#include <usb_com.h>

#define SOMO_CLK  04
#define SOMO_DATA 05

#define LID_SWITCH  12 // high = lid opened
#define ITEM_SWITCH 13 // low = item removed

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

// http://ublo.ro/wixel-pm3-low-power-sleep-mode-cc2511f32/

void setupLidInterrupt()
{
    // clear cpu interrupt flag
    IRCON2 &= ~0x08;    // IRCON2.P1IF = 0
    // clear port 1 interrupt status flag
    P1IFG &= ~0x04;     // P1IFG.P1IF2 = 0
    // set port 1 interrupt mask
    P1IEN |= 0x04;      // P1IEN.P1_2IEN = 1
    // enable rising edge interrupt on port 1
    PICTL &= ~0x02;     // PICTL.P1ICON = 0
    // enable port 1 interrupt
    IEN2 |= 0x10;       // IEN2.P1IE = 1
    // enable global interrupts
    IEN0 |= 0x80;       // IEN0.EA = 1
}

ISR (P1INT, 0) {
    // clear cpu interrupt flag
    IRCON2 &= ~0x08;    // IRCON2.P1IF = 0
    // clear port 1 interrupt status flag
    P1IFG &= ~0x04;     // P1IFG.P1IF2 = 0
   // clear sleep mode 
    SLEEP &= ~0x03; // SLEEP.MODE = 11
    // clear port 1 interrupt mask
    P1IEN &= ~0x04;      // P1IEN.P1_2IEN = 1
    // disable port 1 interrupt
    IEN2 &= ~0x10;                    // clear IEN2.P1IE
    }

void putToSleep()
{
   // select sleep mode PM3
    SLEEP |= 0x03;  // SLEEP.MODE = 11
    // 3 NOPs as specified in 12.1.3
    __asm nop __endasm;
    __asm nop __endasm;
    __asm nop __endasm;
    // enter sleep mode
    if (SLEEP & 0x03) PCON |= 0x01; // PCON.IDLE = 1
}


void main()
{
    systemInit();
    
    // set P1_0 and P1_1 to outputs to avoid leakage current
    setDigitalOutput(10, LOW);
    setDigitalOutput(11, LOW);
    
    setDigitalOutput(SOMO_CLK, HIGH);
    setDigitalOutput(SOMO_DATA, LOW);
    
    if (usbPowerPresent())
    {
        usbInit();
        
        while (1)
        {
            boardService();
            usbComService();
        }
    }
    else
    {
        delayMs(300);

        somoCmd(0x0000);
        
        delayMs(1000);
        setupLidInterrupt();
        putToSleep();
        LED_RED(1);
        
        while(1);
    }
}




