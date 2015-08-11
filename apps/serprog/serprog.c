#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include "serprog.h"

void updateLeds()
{
    usbShowStatusWithGreenLed();
}

enum serprogState { RX_CMD, RX_PARAM, DO_CMD, TX_ANS } state = RX_CMD;
uint8 XDATA ansBuf[33];

void serprogService()
{
  static uint8 cmd = 0;
  static uint8 ansLen, bytesTxed;
  uint8 bytesToTx;
  uint8 i;
  
  switch (state)
  {
  case RX_CMD:
    if (usbComRxAvailable())
    {
      cmd = usbComRxReceiveByte();
      state = DO_CMD;
    }
    break;
    
  case DO_CMD:
    ansBuf[0] = S_ACK;
    
    switch (cmd)
    {
    case S_CMD_NOP:
      // ACK
      ansLen = 1;
      break;
      
    case S_CMD_Q_IFACE:
      // Query programmer iface version
      // ACK + 16bit version
      ansBuf[1] = 1;
      ansBuf[2] = 0;
      ansLen = 3;
      break;
      
    case S_CMD_Q_CMDMAP:
      // Query supported commands bitmap
      // ACK + 32 bytes (256 bits) of supported cmds flags
      ansBuf[1] = CMDMAP_BIT(S_CMD_NOP) | CMDMAP_BIT(S_CMD_Q_IFACE) | CMDMAP_BIT(S_CMD_Q_CMDMAP);
      ansBuf[2] = 0;
      ansBuf[3] = CMDMAP_BIT(S_CMD_SYNCNOP);
      for (i = 4; i < 33; i++)
      {
        ansBuf[i] = 0;
      }
      ansLen = 33;
      break;
      
    case S_CMD_SYNCNOP:
      // NAK + ACK (for synchronization)
      ansBuf[0] = S_NAK;
      ansBuf[1] = S_ACK;
      ansLen = 2;
      break;
      
    default:
      // unsupported command
      // NAK
      ansBuf[0] = S_NAK;
      ansLen = 1;
    }
    bytesTxed = 0;
    state = TX_ANS;
    break;
    
  case TX_ANS:
    if (bytesToTx = usbComTxAvailable())
    {
      if (bytesToTx > (ansLen - bytesTxed))
      {
        bytesToTx = ansLen - bytesTxed;
      }
      usbComTxSend(ansBuf + bytesTxed, bytesToTx);
      bytesTxed += bytesToTx;
      if (bytesTxed == ansLen)
      {
        state = RX_CMD;
      }
    }
    break;
  }
}
/*

#define S_CMD_Q_PGMNAME   0x03    // Query programmer name
#define S_CMD_Q_SERBUF    0x04    // Query Serial Buffer Size
#define S_CMD_Q_BUSTYPE   0x05    // Query supported bustypes

#define S_CMD_Q_OPBUF     0x07    // Query operation buffer size
#define S_CMD_Q_WRNMAXLEN 0x08    // Query Write to opbuf: Write-N maximum length
#define S_CMD_R_BYTE      0x09    // Read a single byte
#define S_CMD_R_NBYTES    0x0A    // Read n bytes
#define S_CMD_O_INIT      0x0B    // Initialize operation buffer
#define S_CMD_O_WRITEB    0x0C    // Write opbuf: Write byte with address
#define S_CMD_O_WRITEN    0x0D    // Write to opbuf: Write-N
#define S_CMD_O_DELAY     0x0E    // Write opbuf: udelay
#define S_CMD_O_EXEC      0x0F    // Execute operation buffer

#define S_CMD_SYNCNOP     0x10    // Special no-operation that returns NAK+ACK

  #define S_CMD_Q_RDNMAXLEN 0x11    // Query read-n maximum length
  #define S_CMD_S_BUSTYPE   0x12    // Set used bustype(s).
  #define S_CMD_O_SPIOP     0x13    // Perform SPI operation.
  #define S_CMD_S_SPI_FREQ  0x14    // Set SPI clock frequency
  #define S_CMD_S_PIN_STATE 0x15    // Enable/disable output drivers
*/

void main()
{
    systemInit();
    usbInit();

    while(1)
    {
      boardService();
      updateLeds();
      usbComService();
      serprogService();
    }
}