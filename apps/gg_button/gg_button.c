#include <wixel.h>
#include <usb.h>
#include <usb_hid.h>

uint8 lastKeyCodeSent = 0;

void updateLeds()
{
    usbShowStatusWithGreenLed();

    //LED_YELLOW(usbHidKeyboardOutput.leds & (1 << LED_CAPS_LOCK));

    // To see the Num Lock or Caps Lock state, you can use these lines instead of the above:
    //LED_YELLOW(usbHidKeyboardOutput.leds & (1 << LED_NUM_LOCK));
    //LED_YELLOW(usbHidKeyboardOutput.leds & (1 << LED_SCROLL_LOCK));

    // NOTE: Reading the Caps Lock, Num Lock, and Scroll Lock states might not work
    // if the USB host is a Linux or Mac OS machine.
}

// See keyboardService for an example of how to use this function correctly.
// Assumption: usbHidKeyboardInputUpdated == 0.  Otherwise, this function
// could clobber a keycode sitting in the buffer that has not been sent to
// the computer yet.
// Assumption: usbHidKeyBoardInput[1 through 5] are all zero.
// Assumption: usbHidKeyboardInput.modifiers is 0.
// NOTE: To send two identical characters, you must send a 0 in between.
void sendKeyCode(uint8 keyCode, uint8 modifiers)
{
    lastKeyCodeSent = keyCode;
    usbHidKeyboardInput.keyCodes[0] = keyCode;
	usbHidKeyboardInput.modifiers = modifiers;

    // Tell the HID library to send the new keyboard state to the computer.
    usbHidKeyboardInputUpdated = 1;
}

// NOTE: This function only handles bouncing that occurs when the button is
// going from the not-pressed to pressed state.
BIT buttonGetSingleDebouncedPress()
{
    static BIT reportedThisButtonPress = 0;
    static uint8 lastTimeButtonWasNotPressed = 0;

    if (!isPinHigh(1))
    {
        // The P0_1 "button" is not pressed.
        reportedThisButtonPress = 0;
        lastTimeButtonWasNotPressed = (uint8)getMs();
    }
    else if ((uint8)(getMs() - lastTimeButtonWasNotPressed) > 15)
    {
        // The P0_1 "button" has been pressed (or at least P0_1 is shorted
        // to 3V3) for 15 ms.

        if (!reportedThisButtonPress)
        {
            reportedThisButtonPress = 1;
            return 1;
        }
    }
    return 0;
}

void keyboardService()
{
    uint8 CODE keyCodeArray[] = {KEY_RETURN, KEY_G, KEY_G, KEY_RETURN, KEY_F10, KEY_N, KEY_S, KEY_Q};
	uint8 CODE modifiersArray[] = {1 << MODIFIER_SHIFT_LEFT, 0, 0, 0, 0, 0, 0, 0};
	
    static uint8 keysLeftToSend = 0;
    static uint8 nextKeyToSend;

    if (buttonGetSingleDebouncedPress() && keysLeftToSend == 0)
    {
        nextKeyToSend = 0;
        keysLeftToSend = sizeof(keyCodeArray);
    }

    LED_RED(keysLeftToSend > 0);

    // Feed data to the HID library, one character at a time.
    if (keysLeftToSend && !usbHidKeyboardInputUpdated)
    {
        uint8 keyCode = keyCodeArray[nextKeyToSend];
		uint8 modifiers = modifiersArray[nextKeyToSend];

        if (keyCode != 0 && keyCode == lastKeyCodeSent)
        {
            // If we need to send the same character twice in a row,
            // send a 0 between them so the compute registers it as
            // two different separate key strokes.
            keyCode = 0;
			modifiers = 0;
        }
        else
        {
            nextKeyToSend++;
            keysLeftToSend--;
        }

        sendKeyCode(keyCode, modifiers);
    }

    // Send a 0 to signal the release of the last key.
    if (keysLeftToSend == 0 && lastKeyCodeSent != 0 && !usbHidKeyboardInputUpdated)
    {
        sendKeyCode(0, 0);
    }
}

void main()
{
    systemInit();
    usbInit();
		
	setPort0PullType(LOW);
	
	if (isPinHigh(1))
	{
		boardStartBootloader();
	}

    while(1)
    {
        updateLeds();
        boardService();
        usbHidService();
        keyboardService();
    }
}
