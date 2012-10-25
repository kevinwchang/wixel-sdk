/** Dependencies **************************************************************/
#include <wixel.h>

#include <usb.h>
#include <usb_com.h>

#include <radio_com.h>
#include <radio_link.h>

/** Parameters ****************************************************************/

/** Global Variables **********************************************************/

/** Functions *****************************************************************/

void main()
{
    BIT pressed = 0;
    
    systemInit();
    usbInit();
    radioComInit();

    while(1)
    {
        boardService();
        radioComTxService();
        usbComService();

        if (!pressed && !isPinHigh(0))
        {
            delayMs(10);
            if (!isPinHigh(0) && radioComTxAvailable())
            {
                pressed = 1;
                radioComTxSendByte('!');
            }
        }
        else if (pressed && isPinHigh(0))
        {
            delayMs(10);
            if (isPinHigh(0))
                pressed = 0;
        }
    }
}
