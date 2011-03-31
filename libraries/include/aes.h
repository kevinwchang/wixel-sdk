#ifndef AES_H_
#define AES_H_

#include <cc2511_types.h>

#define AES_BLOCK_SIZE 16
#define DMA_CHANNEL_AES_IN 2
#define DMA_CHANNEL_AES_OUT 3
#define DMA_CONFIG_AES_IN  dmaConfig._2
#define DMA_CONFIG_AES_OUT dmaConfig._3

void aesInit(void);

void aesLoadKey(uint8 XDATA * key);
void aesLoadIv(uint8 XDATA * iv);

void aesEncrypt(uint8 XDATA * src, uint8 XDATA * dest, uint8 blocks);
void aesDecrypt(uint8 XDATA * src, uint8 XDATA * dest, uint8 blocks);

#endif /* AES_H_ */
