/** Dependencies **************************************************************/
#include <wixel.h>

#include <usb.h>
#include <usb_com.h>

#include <radio_com.h>
#include <radio_link.h>

#include <servo.h>

/** Parameters ****************************************************************/

/** Global Variables **********************************************************/

#define EYES_PIN        10
#define LOWER_SERVO_PIN  2
#define UPPER_SERVO_PIN  3

/** Functions *****************************************************************/


void main()
{
    enum helmetState {CLOSED_EYES_ON, PRE_OPEN_EYES_OFF, OPENING, OPENED, CLOSING, POST_CLOSE_EYES_OFF} state = CLOSED_EYES_ON;
    BIT triggered = 0;
    uint32 timingStartMs = 0;
    uint8 CODE servoPins[] = {LOWER_SERVO_PIN, UPPER_SERVO_PIN};
    
    systemInit();
    usbInit();
    radioComInit();
    
    servosStart((uint8 XDATA *)servoPins, sizeof(servoPins));
    servoSetTarget(0, 867);
    servoSetTarget(1, 1022);    
    servoSetSpeed(0, 1147);
    servoSetSpeed(1, 734);
    
    setDigitalOutput(EYES_PIN, HIGH);

    while(1)
    {
        boardService();
        usbComService();

        triggered = (radioComRxAvailable() && (radioComRxReceiveByte() == (uint8)'!'));
        
        switch (state)
        {
            case CLOSED_EYES_ON:
                if (triggered)
                {
                    setDigitalOutput(EYES_PIN, LOW);
                    timingStartMs = getMs();
                    state = PRE_OPEN_EYES_OFF;
                }
                break;
                
            case PRE_OPEN_EYES_OFF:
                if (getMs() - timingStartMs > 500)
                {
                    servoSetTarget(0, 2400);
                    servoSetTarget(1, 1985);
                    state = OPENING;
                    //LED_YELLOW(1);
                }
                break;
                
            case OPENING:
                if (!servosMoving())
                {
                    state = OPENED;
                    //LED_YELLOW(0);
                }
                break;
                
            case OPENED:
                if (triggered)
                {
                    servoSetTarget(0, 867);
                    servoSetTarget(1, 1022);
                    state = CLOSING;
                    //LED_YELLOW(1);
                }
                break;
                
            case CLOSING:
                if (!servosMoving())
                {
                    timingStartMs = getMs();
                    state = POST_CLOSE_EYES_OFF;
                    //LED_YELLOW(0);
                }
                break; 
                
            case POST_CLOSE_EYES_OFF:
                if (getMs() - timingStartMs > 500)
                {
                    setDigitalOutput(EYES_PIN, HIGH);
                    state = CLOSED_EYES_ON;
                }
                break;
        }
    }
}
