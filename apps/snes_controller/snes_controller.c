#include <wixel.h>
#include <usb.h>
#include <usb_hid.h>

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

uint16 DATA buttons = 0, prevButtons = 0; // each bit is 1 if pressed

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

void gamePadService()
{
	if ((buttons & (1 << BUTTON_LEFT)) && !(buttons & (1 << BUTTON_RIGHT)))
		usbHidJoystickInput.x = -127;
	else if ((buttons & (1 << BUTTON_RIGHT)) && !(buttons & (1 << BUTTON_LEFT)))
		usbHidJoystickInput.x = 127;
	else
		usbHidJoystickInput.x = 0;
	
	if ((buttons & (1 << BUTTON_UP)) && !(buttons & (1 << BUTTON_DOWN)))
		usbHidJoystickInput.y = -127;
	else if ((buttons & (1 << BUTTON_DOWN)) && !(buttons & (1 << BUTTON_UP)))
		usbHidJoystickInput.y = 127;
	else
		usbHidJoystickInput.y = 0;
	
	usbHidJoystickInput.buttons = ((((buttons >> BUTTON_START ) & 1) << 0) |
	                               (((buttons >> BUTTON_SELECT) & 1) << 1) |
	                               (((buttons >> BUTTON_A     ) & 1) << 2) |
	                               (((buttons >> BUTTON_B     ) & 1) << 3) |
	                               (((buttons >> BUTTON_X     ) & 1) << 4) |
	                               (((buttons >> BUTTON_Y     ) & 1) << 5) |
	                               (((buttons >> BUTTON_L     ) & 1) << 6) |
	                               (((buttons >> BUTTON_R     ) & 1) << 7));
	
	usbHidJoystickInputUpdated = 1;
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
			
			readController();
			
			if (buttons != prevButtons)
			{
				gamePadService();
				prevButtons = buttons;
			}
			usbHidService();
		}
	}
}
