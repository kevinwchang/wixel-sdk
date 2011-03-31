#include <cc2511_map.h>
#include <cc2511_types.h>
#include <dma.h>
#include <aes.h>

#define AES_ENCRYPT 0
#define AES_DECRYPT 1

#define AES_MODE 0b000 // CBC

void aesInit(void)
{
    DMA_CONFIG_AES_IN.DESTADDRH = XDATA_SFR_ADDRESS(ENCDI) >> 8; // AES data input
    DMA_CONFIG_AES_IN.DESTADDRL = XDATA_SFR_ADDRESS(ENCDI);
    // LENL is set by aesEncrypt() and aesDecrypt()
    DMA_CONFIG_AES_IN.VLEN_LENH = 0; // use LEN for transfer count
    DMA_CONFIG_AES_IN.DC6 = 29; // WORDSIZE = 0, TMODE = 0, TRIG = 29 (AES input request)
    DMA_CONFIG_AES_IN.DC7 = 0x40; // SRCINC = 1, DESTINC = 0, IRQMASK = 0, M8 = 0, PRIORITY = 0

    DMA_CONFIG_AES_OUT.SRCADDRH = XDATA_SFR_ADDRESS(ENCDO) >> 8; // AES data output
    DMA_CONFIG_AES_OUT.SRCADDRL = XDATA_SFR_ADDRESS(ENCDO);
    // LENL is set by aesEncrypt() and aesDecrypt()
    DMA_CONFIG_AES_OUT.VLEN_LENH = 0; // use LEN for transfer count
    DMA_CONFIG_AES_OUT.DC6 = 30; // WORDSIZE = 0, TMODE = 0, TRIG = 30 (AES output request)
    DMA_CONFIG_AES_OUT.DC7 = 0x10; // SRCINC = 0, DESTINC = 1, IRQMASK = 0, M8 = 0, PRIORITY = 0
}

void aesLoadKey(uint8 XDATA * key)
{
    uint8 i;

    ENCCS = 0x05; // ENCCS.CMD (2:1) = 10 (Load key), ENCCS.ST (0)  = 1 (start)
    for (i = 0; i < AES_BLOCK_SIZE; i++)
    {
        ENCDI = key[i];
    }

    // wait until key is downloaded
    while(!(ENCCS & 0x08)); // ENCCS.RDY (3)
}

void aesLoadIv(uint8 XDATA * iv)
{
    uint8 i;

    ENCCS = 0x07; // ENCCS.CMD (2:1) = 11 (Load IV/nonce), ENCCS.ST (0)  = 1 (start)
    for (i = 0; i < AES_BLOCK_SIZE; i++)
    {
        ENCDI = iv[i];
    }

    // wait until IV is downloaded
    while(!(ENCCS & 0x08)); // ENCCS.RDY (3)
}

void aesProcess(uint8 XDATA * src, uint8 XDATA * dest, uint8 blocks, uint8 dir)
{
    // configure DMA transfer length and src/dest addresses
    DMA_CONFIG_AES_IN.LENL = blocks * AES_BLOCK_SIZE;
    DMA_CONFIG_AES_OUT.LENL = blocks * AES_BLOCK_SIZE;
    DMA_CONFIG_AES_IN.SRCADDRH = (unsigned int)src >> 8;
    DMA_CONFIG_AES_IN.SRCADDRL = (unsigned int)src;
    DMA_CONFIG_AES_OUT.DESTADDRH = (unsigned int)dest >> 8;
    DMA_CONFIG_AES_OUT.DESTADDRL = (unsigned int)dest;

    // arm DMA channel
    DMAARM |= ((1 << DMA_CHANNEL_AES_IN) | (1 << DMA_CHANNEL_AES_OUT));

    // clear AES interrupt flags
    ENCIF_1 = ENCIF_0 = 0;

    // start AES processor
    ENCCS = (AES_MODE << 4) | (dir << 1) | 1; // ENCCS.MODE (6:4) = AES_MODE; ENCCS.CMD (2:1) = dir (00 for encrypt, 01 for decrypt); ENCCS.ST (0) = 1

    // wait for processing to complete
    while (!ENCIF_0);
}

void aesEncrypt(uint8 XDATA * src, uint8 XDATA * dest, uint8 blocks)
{
    aesProcess(src, dest, blocks, AES_ENCRYPT);
}

void aesDecrypt(uint8 XDATA * src, uint8 XDATA * dest, uint8 blocks)
{
    aesProcess(src, dest, blocks, AES_DECRYPT);
}

