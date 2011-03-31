/* wireless_serial_encrypted app:
 *
 * Pin out:
 * P1_5 = Radio Transmit Debug Signal
 * P1_6 = Serial TX (0-3.3V)
 * P1_7 = Serial RX (0-3.3V, not 5V tolerant)
 *
 * == Overview ==
 * This app allows you to connect two Wixels together to make a wireless,
 * bidirectional, lossless serial link.  The Wixels must be on the same radio
 * channel, and all other pairs of Wixels must be at least 2 channels away.
 * Basic encryption is applied to the radio link, which can foil casual
 * eavesdropping by sniffing radio packets, but it is probably inadequate for
 * situations where security is critical.
 *
 * == Technical Description ==
 * This device appears to the USB host as a Virtual COM Port, with USB product
 * ID 0x2200.  It uses the radio_link library to do wireless communication.
 *
 * There are three basic serial modes that can be selected:
 * 1) USB-to-Radio: Bytes from the USB virtual COM port get sent to the
 *    radio and vice versa.
 * 2) UART-to-Radio: Bytes from the UART's RX line get sent to the radio
 *    and bytes from the radio get sent to the UART's TX line.
 * 3) USB-to-UART: Just like a normal USB-to-Serial adapter, bytes from
 *    the virtual COM port get sent on the UART's TX line and bytes from
 *    the UART's RX line get sent to the virtual COM port.
 *
 * You can select which serial mode you want to use by setting the serial_mode
 * parameter to the appropriate number (using the numbers above).  Or, you can
 * leave the serial mode at 0 (which is the default).  If the serial_mode is 0,
 * then the Wixel will automatically choose a serial mode based on how it is
 * being powered, and it will switch between the different serial modes on the
 * fly.
 *
 * Power Source | Serial Mode
 * --------------------------
 * USB only     | USB-to-Radio
 * VIN only     | UART-to-Radio
 * USB and VIN  | USB-to-UART
 *
 * This app uses a custom radio_com library that encrypts radio data before
 * sending it. The encryption is 128-bit AES in CBC mode, with a user-configured
 * key and IV.  For simplicity, each packet contains one 128-bit (16 byte) AES
 * block. The data in each block is padded to exactly 16 bytes before being
 * encrypted; the 16th byte is equal to the number of padding bytes, and the
 * rest of the padding contains random bytes (so there is always at least one
 * byte of padding, and a block/packet can contain at most 15 data bytes).
 *
 * Of course, if an attacker has physical access to a Wixel running this app,
 * he can just read the key and IV from the Wixel, or intercept the unencrypted
 * data on USB or UART, so the encryption only provides (some) protection
 * against wireless sniffing alone.
 *
 * Possible improvements to enhance the security of the radio encryption:
 *
 * - Randomly generate an IV and distribute it to the other side of the link
 *      instead of using the same user-configured IV every time.
 *
 * - Transmit dummy packets at random intervals to make it harder to see
 *      patterns in the radio data. (For example, keystrokes usually cause
 *      packets with a single byte of data to be transmitted, one per character;
 *      this makes it possible to guess the length of a password by paying
 *      attention to packet timing.)
 *
 * == Parameters ==
 *   serial_mode   : Selects the serial mode or auto mode (0-3).
 *   baud_rate     : The baud rate to use for the UART, in bits per second.
 *   radio_channel : See description in radio_link.h.
 *   encryption_key_[0-4] : The 128-bit encryption key, split into four 32-bit
 *                          (signed int) chunks. (Possible values are
 *                          -2147483648 to 2147483647)
 *   encryption_IV_[0-4]  : The 128-bit encryption IV (nonce), split into four
 *                          32-bit (signed int) chunks. (Possible values are
 *                          -2147483648 to 2147483647)
 *
 * == Example Uses ==
 * 1) This application can be used to make a wireless serial link between two
 *    microcontrollers, with no USB involved.  To do this, use the UART-to-Radio
 *    mode on both Wixels.
 *
 * 2) This application can be used to make a wireless serial link between a
 *    computer and a microcontroller.  Use USB-to-Radio mode on the Wixel that
 *    is connected to the computer and use UART-to-Radio mode on the Wixel
 *    that is connected to the microcontroller.
 *
 * 3) If you are doing option 2 and using the the auto-detect serial mode
 *    (serial_mode = 0), then you have the option to (at any time) plug a USB
 *    cable directly in to the Wixel that is connected to your microcontroller
 *    to establish a more direct (wired) serial connection with the
 *    microcontroller.  (You would, of course, also have to switch to the other
 *    COM port when you do this.)
 */

// TODO: try harder to enable a pull-up on the RX line.  Right now, we get junk
// on the RX line from 60 Hz noise if you connect one end of a cable to the RX line.

/*
 * TODO: Support for USB CDC ACM control signals.
 * TODO: use LEDs to give feedback about sending/receiving bytes.
 * TODO: UART flow control.
 * TODO: Better radio protocol (see TODOs in radio_link.c).
 * TODO: Obey CDC-ACM Set Line Coding commands:
 *       In USB-UART mode this would let the user change the baud rate at run-time.
 *       In USB-RADIO mode, bauds 0-255 would correspond to radio channels.
 * TODO: Allow a "password" parameter as a string and generate the key (and
 *       possibly the IV) based on it.
 */

/** Dependencies **************************************************************/
#include <cc2511_map.h>
#include <board.h>
#include <random.h>
#include <time.h>

#include <usb.h>
#include <usb_com.h>

#include "encrypted_radio_com.h"
#include <radio_link.h>

#include <uart1.h>

#include <aes.h>
#include <string.h>

/** Parameters ****************************************************************/
#define SERIAL_MODE_AUTO        0
#define SERIAL_MODE_USB_RADIO   1
#define SERIAL_MODE_UART_RADIO  2
#define SERIAL_MODE_USB_UART    3
int32 CODE param_serial_mode = SERIAL_MODE_AUTO;

int32 CODE param_baud_rate = 9600;

// defined in aes_params.s
extern int32 CODE encryption_key[4];
extern int32 CODE encryption_IV[4];

/** Functions *****************************************************************/
void updateLeds()
{
    usbShowStatusWithGreenLed();

    LED_YELLOW(vinPowerPresent());

    // Turn on the red LED if the radio is in the RX_OVERFLOW state.
    // There used to be several bugs in the radio libraries that would cause
    // the radio to go in to this state, but hopefully they are all fixed now.
    if (MARCSTATE == 0x11)
    {
        LED_RED(1);
    }
    else
    {
        LED_RED(0);
    }
}

uint8 currentSerialMode()
{
    if ((uint8)param_serial_mode > 0 && (uint8)param_serial_mode <= 3)
    {
        return (uint8)param_serial_mode;
    }

    if (usbPowerPresent())
    {
        if (vinPowerPresent())
        {
            return SERIAL_MODE_USB_UART;
        }
        else
        {
            return SERIAL_MODE_USB_RADIO;
        }
    }
    else
    {
        return SERIAL_MODE_UART_RADIO;
    }
}

void usbToRadioService()
{
    while(usbComRxAvailable() && radioComTxAvailable())
    {
        radioComTxSendByte(usbComRxReceiveByte());
    }

    while(radioComRxAvailable() && usbComTxAvailable())
    {
        usbComTxSendByte(radioComRxReceiveByte());
    }
}

void uartToRadioService()
{
    while(uart1RxAvailable() && radioComTxAvailable())
    {
        radioComTxSendByte(uart1RxReceiveByte());
    }

    while(radioComRxAvailable() && uart1TxAvailable())
    {
        uart1TxSendByte(radioComRxReceiveByte());
    }
}

void usbToUartService()
{
    while(usbComRxAvailable() && uart1TxAvailable())
    {
        uart1TxSendByte(usbComRxReceiveByte());
    }

    while(uart1RxAvailable() && usbComTxAvailable())
    {
        usbComTxSendByte(uart1RxReceiveByte());
    }
}

void main()
{
    systemInit();
    usbInit();

    uart1Init();
    uart1SetBaudRate(param_baud_rate);

    radioComInit();
    randomSeedFromAdc();

    aesInit();
    aesLoadKey((uint8 XDATA *)encryption_key);
    aesLoadIv((uint8 XDATA *)encryption_IV);

    // Set up P1_5 to be the radio's TX debug signal.
    P1DIR |= (1<<5);
    IOCFG0 = 0b011011; // P1_5 = PA_PD (TX mode)

    while(1)
    {
        boardService();
        updateLeds();

        radioComTxService();
        usbComService();

        switch(currentSerialMode())
        {
        case SERIAL_MODE_USB_RADIO:  usbToRadioService();  break;
        case SERIAL_MODE_UART_RADIO: uartToRadioService(); break;
        case SERIAL_MODE_USB_UART:   usbToUartService();   break;
        }
    }
}
