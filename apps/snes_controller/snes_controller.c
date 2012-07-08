/** example_blink_led app:
This app blinks the red LED.

For a precompiled version of this app and a tutorial on how to load this app
onto your Wixel, see the Pololu Wixel User's Guide:
http://www.pololu.com/docs/0J46
*/

#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

#define PIN_CLOCK 10
#define PIN_LATCH 11
#define PIN_DATA  12

#define BUTTON_B      0
#define BUTTON_Y      1
#define BUTTON_SELECT 2
#define BUTTON_START  3
#define BUTTON_UP     4
#define BUTTON_DOWN   5
#define BUTTON_LEFT   6
#define BUTTON_RIGHT  7
#define BUTTON_A      8
#define BUTTON_X      9
#define BUTTON_L     10
#define BUTTON_R     11

uint16 DATA buttons = 0, prev_buttons = 0; // each bit is 1 if pressed

void updateLeds()
{
	usbShowStatusWithGreenLed();

	LED_YELLOW(0);
	LED_RED(buttons);
}

void controllerInit()
{
	setDigitalOutput(PIN_LATCH, LOW);
	setDigitalOutput(PIN_CLOCK, HIGH);
	setDigitalInput(PIN_DATA, HIGH_IMPEDANCE);
}

void readController()
{
	uint8 i;
	buttons = 0;
	
	EA = 0; // disable interrupts
	
	setDigitalOutput(PIN_LATCH, HIGH);
	delayMicroseconds(12);
	setDigitalOutput(PIN_LATCH, LOW);
	delayMicroseconds(6);
	
	for (i = 0; i < 16; i++)
	{
		buttons |= ((!isPinHigh(PIN_DATA)) << i);
		
		setDigitalOutput(PIN_CLOCK, LOW);
		delayMicroseconds(6);
		setDigitalOutput(PIN_CLOCK, HIGH);
		delayMicroseconds(6);
	}
	
	EA = 1;
}

#define LOOP_INTERVAL 5 // ms

void main()
{
	uint8 lastLoop = 0;
	
	systemInit();
	usbInit();
	controllerInit();
	
	while(1)
	{
		if ((uint8)(getMs() - lastLoop) > LOOP_INTERVAL)
		{
			boardService();
			updateLeds();
			usbComService();
			
			readController();
		}
	}
}
