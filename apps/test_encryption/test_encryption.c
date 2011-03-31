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
 * == Parameters ==
 *   serial_mode   : Selects the serial mode or auto mode (0-3).
 *   baud_rate     : The baud rate to use for the UART, in bits per second.
 *   radio_channel : See description in radio_link.h.
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
 */

/** Dependencies **************************************************************/
#include <cc2511_map.h>
#include <board.h>
#include <time.h>
#include <usb.h>
#include <usb_com.h>
#include <aes.h>
#include <stdio.h>
#include <ctype.h>

/** Parameters ****************************************************************/

/** Functions *****************************************************************/
#define CRLF() { usbComTxSendByte('\r'); usbComTxSendByte('\n'); }

void updateLeds()
{
    usbShowStatusWithGreenLed();

    LED_YELLOW(vinPowerPresent());
}


void putchar(char c)
{
    usbComTxSendByte(c);
}

void periodicTasks(void)
{
    boardService();
    updateLeds();
    usbComService();
}

void waitForTx(void)
{
    while (usbComTxAvailable() < 64)
    {
        periodicTasks();
    }
}
void waitForRxAndTx(void)
{
    while (usbComRxAvailable() < 1 || usbComTxAvailable() < 64)
    {
        periodicTasks();
    }
}

uint8 asciiToNibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        c -= '0';
    }
    else if (c >= 'A' && c <= 'F')
    {
        c -= ('A' - 10);
    }
    else if (c >= 'a' && c <= 'f')
    {
        c -= ('a' - 10);
    }
    return c;
}

uint8 asciiToByte(char highNibble, char lowNibble)
{
    return (asciiToNibble(highNibble) << 4 | asciiToNibble(lowNibble));
}

BIT read128BitHex(uint8 XDATA * buf)
{
    uint8 XDATA hexBuf[32];
    uint8 i = 0;
    char c;

    while (i < 32)
    {
        waitForRxAndTx();
        c = usbComRxReceiveByte();
        if (c == 3) // ctrl-c
        {
            return 0;
        }
        else if (isxdigit(c))
        {
            usbComTxSendByte(c); // echo
            hexBuf[i++] = c;
        }
    }

    for (i = 0; i < 16; i++)
    {
        buf[i] = asciiToByte(hexBuf[2 * i], hexBuf[2 * i + 1]);
    }
    return 1;
}

char nibbleToAscii(uint8 nibble)
{
    nibble &= 0xF;
    if (nibble <= 0x9)
    {
        return '0' + nibble;
    }
    else
    {
        return 'A' + (nibble - 10);
    }
}

void print128BitHex(uint8 XDATA * buf)
{
    uint8 i;

    waitForTx();

    for (i = 0; i < 16; i++)
    {
        usbComTxSendByte(nibbleToAscii(buf[i] >> 4));
        usbComTxSendByte(nibbleToAscii(buf[i]));
    }
}

void main()
{
    char mode = 0;
    uint8 XDATA buf[16];

    systemInit();
    usbInit();

    aesInit();

    while(1)
    {
        waitForTx();
        CRLF();
        mode = 0;

        while (mode != 'e' && mode != 'd')
        {
            waitForTx();
            CRLF();
            printf("Mode (e = encrypt, d = decrypt): ");

            waitForRxAndTx();
            mode = usbComRxReceiveByte();
            usbComTxSendByte(mode); // echo
        }

        waitForTx();
        CRLF();
        printf("Key: ");
        if (!read128BitHex(buf)) { continue; }
        aesLoadKey(buf);

        waitForTx();
        CRLF();
        printf("IV: ");
        if (!read128BitHex(buf)) { continue; }
        aesLoadIv(buf);

        waitForTx();
        CRLF();
        if (mode == 'e')
        {
            printf("Plaintext: ");
        }
        else
        {
            printf("Ciphertext: ");
        }
        if (!read128BitHex(buf)) { continue; }

        waitForTx();
        CRLF();
        if (mode == 'e')
        {
            aesEncrypt(buf, buf, 1);
            printf("Ciphertext = ");
        }
        else
        {
            aesDecrypt(buf, buf, 1);
            printf("Plaintext = ");
        }
        print128BitHex(buf);
    }
}
