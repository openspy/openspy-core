#ifndef _BUFF_WRITE_INC
#define _BUFF_WRITE_INC
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "chc_endian.h"
void BufferWriteByte(uint8_t **buffer, uint32_t *len, uint8_t value);
void BufferWriteInt(uint8_t **buffer, uint32_t *len, uint32_t value);
void BufferWriteIntRE(uint8_t **buffer, uint32_t *len, uint32_t value);
void BufferWriteShort(uint8_t **buffer, uint32_t *len, uint16_t value);
void BufferWriteShortRE(uint8_t **buffer, uint32_t *len, uint16_t value);
void BufferWriteData(uint8_t **buffer, uint32_t *len, uint8_t *data, uint32_t writelen);
void BufferWriteNTS(uint8_t **buffer, uint32_t *len, uint8_t *string);
#endif
