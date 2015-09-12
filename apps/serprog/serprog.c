#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <spi0_master.h>
#include <stdio.h>
#include "serprog.h"

#define SERBUFMAXLEN  16
#define WRNMAXLEN     200
#define RDNMAXLEN     200

#define SPI_SS 4
#define SPI_SELECT()    (setDigitalOutput(SPI_SS, LOW))
#define SPI_DESELECT()  (setDigitalOutput(SPI_SS, HIGH))

void updateLeds()
{
    usbShowStatusWithGreenLed();
}

enum serprogState { RX_CMD, RX_PARAM, DO_CMD, TX_ANS } state = RX_CMD;

static uint8 XDATA params[SERBUFMAXLEN];
//static uint8 XDATA wrnBuf[WRNMAXLEN];
//static uint8 XDATA rdnBuf[RDNMAXLEN];
static uint8 XDATA ansBuf[1 + RDNMAXLEN];

void serprogService()
{
  static uint8 cmd = 0;
  static uint8 paramLen, bytesRxed;
  static uint8 XDATA * rxPtr;
  static uint8 rlen;
  static uint8 ansLen, bytesTxed;
  static uint8 XDATA b = 0;
  uint8 bytesToTx;
  uint8 i;
  
  switch (state)
  {
  case RX_CMD:
    if (usbComRxAvailable())
    {
      cmd = usbComRxReceiveByte();
      bytesRxed = 0;
      rxPtr = params;
      state = RX_PARAM;
      
      switch (cmd)
      {
      case S_CMD_S_BUSTYPE:
        paramLen = 1;
        break;
        
      case S_CMD_O_SPIOP:
        paramLen = 6;
        rlen = 0;
        break;
        
      default:
        state = DO_CMD;
      }
    }
    break;
  
  case RX_PARAM:
    while (usbComRxAvailable() && bytesRxed < paramLen)
    {
      *rxPtr = usbComRxReceiveByte();
      rxPtr++;
      bytesRxed++;
      
      switch (cmd)
      {
      case S_CMD_O_SPIOP: // 0x13
        if (bytesRxed == 3)
        {
          // assumption: slen <= WRNMAXLEN
          paramLen += params[0];
          rxPtr = params;
        }
        else if (bytesRxed == 6)
        {
          // assumption: rlen <= RDNMAXLEN
          rlen = params[0];
          rxPtr = &b;
          SPI_SELECT();
        }
        else if (bytesRxed > 6)
        {
          spi0MasterSendByte(b);
          rxPtr = &b;
        }
        break;
      
      default:
        // do nothing
      }
    }
    if (bytesRxed == paramLen)
    {
      state = DO_CMD;
    }
    break;
    
  case DO_CMD:
    ansBuf[0] = S_ACK;
    
    switch (cmd)
    {
    case S_CMD_NOP: // 0x00
      // ACK
      ansLen = 1;
      break;
      
    case S_CMD_Q_IFACE: // 0x01
      // Query programmer iface version
      // ACK + 16bit version (nonzero)
      ansBuf[1] = 1;
      ansBuf[2] = 0;
      ansLen = 3;
      break;
      
    case S_CMD_Q_CMDMAP: // 0x02
      // Query supported commands bitmap
      // ACK + 32 bytes (256 bits) of supported cmds flags
      ansBuf[1] = CMDMAP_BIT(S_CMD_NOP) | CMDMAP_BIT(S_CMD_Q_IFACE) | CMDMAP_BIT(S_CMD_Q_CMDMAP) | CMDMAP_BIT(S_CMD_Q_PGMNAME) | CMDMAP_BIT(S_CMD_Q_SERBUF) | CMDMAP_BIT(S_CMD_Q_BUSTYPE);
      ansBuf[2] = CMDMAP_BIT(S_CMD_Q_WRNMAXLEN);
      ansBuf[3] = CMDMAP_BIT(S_CMD_SYNCNOP) | CMDMAP_BIT(S_CMD_Q_RDNMAXLEN) | CMDMAP_BIT(S_CMD_S_BUSTYPE) | CMDMAP_BIT(S_CMD_O_SPIOP);
      for (i = 4; i < 33; i++)
      {
        ansBuf[i] = 0;
      }
      ansLen = 33;
      break;
    
    case S_CMD_Q_PGMNAME: // 0x03
      // Query programmer name
      // ACK + 16 bytes string (null padding) / NAK
      i = sprintf(ansBuf + 1, "Wixel serprog\0\0\0");
      ansLen = 17;
      break;
    
    case S_CMD_Q_SERBUF: // 0x04
      // Query serial buffer size
      // ACK + 16bit size / NAK
      ansBuf[1] = 16;
      ansBuf[2] = 0;
      ansLen = 3;
      break;
    
    case S_CMD_Q_BUSTYPE: // 0x05
      // Query supported bustypes
      // ACK + 8-bit flags (as per flashrom) / NAK
      ansBuf[1] = BUSTYPE_SPI;
      ansLen = 2;
      break;    
      
    case S_CMD_Q_WRNMAXLEN: // 0x08
      // Query maximum write-n length
      // ACK + 24bit length (0==2^24) / NAK
      ansBuf[1] = WRNMAXLEN;
      ansBuf[2] = 0;
      ansBuf[3] = 0;
      ansLen = 4;
      break;
      
    case S_CMD_SYNCNOP: // 0x10
      // NAK + ACK (for synchronization)
      ansBuf[0] = S_NAK;
      ansBuf[1] = S_ACK;
      ansLen = 2;
      break;
    
    case S_CMD_Q_RDNMAXLEN: // 0x11
      // Query maximum read-n length
      // ACK + 24-bit length (0==2^24) / NAK
      ansBuf[1] = RDNMAXLEN;
      ansBuf[2] = 0;
      ansBuf[3] = 0;
      ansLen = 4;
      break;
      
    case S_CMD_S_BUSTYPE: // 0x12
      // Set used bustype
      // ACK / NAK
      if (params[0] != BUSTYPE_SPI)
      {
        ansBuf[0] = S_NAK;
      }
      ansLen = 1;
      break;
      
    case S_CMD_O_SPIOP: // 0x13
      // Perform SPI operation
      // ACK + rlen bytes of data / NAK
      for (i = 0; i < rlen; i++)
      {
        ansBuf[1 + i] = spi0MasterReceiveByte();
      }
      SPI_DESELECT();
      ansLen = 1 + rlen;
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


#define S_CMD_Q_OPBUF     0x07    // Query operation buffer size
#define S_CMD_R_BYTE      0x09    // Read a single byte
#define S_CMD_R_NBYTES    0x0A    // Read n bytes
#define S_CMD_O_INIT      0x0B    // Initialize operation buffer
#define S_CMD_O_WRITEB    0x0C    // Write opbuf: Write byte with address
#define S_CMD_O_WRITEN    0x0D    // Write to opbuf: Write-N
#define S_CMD_O_DELAY     0x0E    // Write opbuf: udelay
#define S_CMD_O_EXEC      0x0F    // Execute operation buffer

#define S_CMD_SYNCNOP     0x10    // Special no-operation that returns NAK+ACK


  #define S_CMD_S_SPI_FREQ  0x14    // Set SPI clock frequency
  #define S_CMD_S_PIN_STATE 0x15    // Enable/disable output drivers
*/

void main()
{
    systemInit();
    usbInit();
    SPI_DESELECT();
    spi0MasterInit();
    spi0MasterSetFrequency(3000000);
    spi0MasterSetClockPolarity(SPI_POLARITY_IDLE_LOW);
    spi0MasterSetClockPhase(SPI_PHASE_EDGE_LEADING);
    spi0MasterSetBitOrder(SPI_BIT_ORDER_MSB_FIRST);

    while(1)
    {
      boardService();
      updateLeds();
      usbComService();
      serprogService();
    }
}