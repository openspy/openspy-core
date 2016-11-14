#ifndef _BUFF_WRITE_INC
#define _BUFF_WRITE_INC
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "chc_endian.h"
void BufferWriteByte(uint8_t **buffer, int *len, const uint8_t value);
void BufferWriteInt(uint8_t **buffer, int *len, const uint32_t value);
void BufferWriteIntRE(uint8_t **buffer, int *len, const uint32_t value);
void BufferWriteShort(uint8_t **buffer, int *len, const uint16_t value);
void BufferWriteShortRE(uint8_t **buffer, int *len, const uint16_t value);
void BufferWriteData(uint8_t **buffer, int *len, const uint8_t *data, int writelen);
void BufferWriteNTS(uint8_t **buffer, int *len, const uint8_t *string);
#endif
