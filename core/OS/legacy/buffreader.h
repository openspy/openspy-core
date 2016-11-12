#ifndef _BUFF_READ_INC
#define _BUFF_READ_INC
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "chc_endian.h"

uint8_t BufferReadByte(uint8_t **buffer, uint32_t *len);
uint32_t BufferReadInt(uint8_t **buffer, uint32_t *len);
uint32_t BufferReadIntRE(uint8_t **buffer, uint32_t *len);
uint16_t BufferReadShort(uint8_t **buffer, uint32_t *len);
uint16_t BufferReadShortRE(uint8_t **buffer, uint32_t *len);
uint8_t *BufferReadData(uint8_t **buffer, uint32_t *len, uint32_t readlen); //allocates memory
uint8_t *BufferReadData(uint8_t **buffer, uint32_t *len, uint8_t *dst, uint32_t readlen);
uint8_t *BufferReadNTS(uint8_t **buffer, uint32_t *len); //allocates memory
#endif
