#include <radio_link.h>
#include <aes.h>
#include <random.h>
#include "encrypted_radio_com.h"

static uint8 DATA txBytesLoaded = 0;
static uint8 DATA rxBytesLeft = 0;

static uint8 XDATA * DATA rxPointer = 0;
static uint8 XDATA * DATA txPointer = 0;
static uint8 XDATA * DATA packetPointer = 0;

// For highest throughput, we want to send as much data in each packet
// as possible.  But for lower latency, we sometimes need to send packets
// that are NOT full.
// This library will only send non-full packets if the number of packets
// currently queued to be sent is small.  Specifically, that number must
// not exceed TX_QUEUE_THRESHOLD.
// A higher threshold means that there will be more under-populated packets
// at the beginning of a data transfer (which is bad), but slightly reduces
// the importance of calling radioComTxService often (which can be good).
#define TX_QUEUE_THRESHOLD  1

void radioComInit()
{
    radioLinkInit();
}

// NOTE: This function only returns the number of bytes available in the CURRENT PACKET.
// It doesn't look at all the packets received, and it doesn't count the data that is
// queued on the other Wixel.  Therefore, it is never recommended to write some kind of
// program that waits for radioComRxAvailable to reach some value greater than 1: it might
// never reach that value.
uint8 radioComRxAvailable(void)
{
    if (rxBytesLeft != 0)
    {
        return rxBytesLeft;
    }

    rxPointer = radioLinkRxCurrentPacket();

    if (rxPointer == 0)
    {
        return 0;
    }

    aesDecrypt(rxPointer + 1, rxPointer + 1, 1);
    rxBytesLeft = AES_BLOCK_SIZE - rxPointer[1 + AES_BLOCK_SIZE - 1]; // Compute the data length. The last byte contains the number of padding bytes.
    rxPointer += 1;              // Make rxPointer point to the data.

    // Assumption: radioLink doesn't ever return zero-length packets,
    // so rxBytesLeft is non-zero now and we don't have to worry about
    // discard zero-length packets in radio_com.c.

    return rxBytesLeft;
}

// Assumption: The user recently called radioComRxAvailable and it returned
// a non-zero value.
uint8 radioComRxReceiveByte(void)
{
    uint8 tmp = *rxPointer;   // Read a byte from the current RX packet.
    rxPointer++;              // Update pointer and counter.
    rxBytesLeft--;

    if (rxBytesLeft == 0)     // If there are no bytes left in this packet...
    {
        radioLinkRxDoneWithPacket();  // Tell the radio link layer we are done with it so we can receive more.
    }

    return tmp;
}

// pad data with random bytes, except for the last byte which is equal to the number of padding bytes.
// (There is always at least 1 padding byte, so the max data length is 15.)
static void padTxData(void)
{
    // random bytes
    while (txPointer < (packetPointer + 1 + AES_BLOCK_SIZE - 1))
    {
        txPointer++;
        *txPointer = randomNumber();
    }

    // last byte: padding length
    packetPointer[1 + AES_BLOCK_SIZE - 1] = AES_BLOCK_SIZE - txBytesLoaded;
}

static void radioComEncryptAndSendPacketNow(void)
{
    padTxData();
    aesEncrypt(packetPointer + 1, packetPointer + 1, 1);
    *packetPointer = AES_BLOCK_SIZE;
    radioLinkTxSendPacket();
    txBytesLoaded = 0;
}

void radioComTxService(void)
{
    if (txBytesLoaded != 0 && radioLinkTxQueued() <= TX_QUEUE_THRESHOLD)
    {
        radioComEncryptAndSendPacketNow();
    }
}


uint8 radioComTxAvailable(void)
{
    // Assumption: If txBytesLoaded is non-zero, radioLinkTxAvailable will be non-zero,
    // so the subtraction below does not overflow.
    // Assumption: The multiplication below does not overflow ever.
    return radioLinkTxAvailable() * (AES_BLOCK_SIZE - 1) - txBytesLoaded;
}

void radioComTxSendByte(uint8 byte)
{
    // Assumption: The user called radioComTxAvailable recently and it returned a non-zero value.
    if (txBytesLoaded == 0)
    {
        txPointer = packetPointer = radioLinkTxCurrentPacket();
    }

    txPointer++;
    *txPointer = byte;
    txBytesLoaded++;

    if (txBytesLoaded == (AES_BLOCK_SIZE - 1)) // reserve 1 byte for padding
    {
        radioComEncryptAndSendPacketNow();
    }
}

void radioComTxSend(const uint8 XDATA * buffer, uint8 size);
