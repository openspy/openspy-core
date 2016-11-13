#define _CRT_SECURE_NO_WARNINGS //TODO: make buffer state to hold alloc len, etc so can strcpy_s
#include "buffwriter.h"
void BufferWriteByte(uint8_t **buffer, int *len, uint8_t value) {
	**buffer = value;
	*len += sizeof(uint8_t);
	*buffer += sizeof(uint8_t);
}
void BufferWriteInt(uint8_t **buffer, int *len, uint32_t value) {
	**((uint32_t **)buffer) = value;
	*len += sizeof(uint32_t);
	*buffer += sizeof(uint32_t);
}
void BufferWriteIntRE(uint8_t **buffer, int *len, uint32_t value) {
	BufferWriteInt(buffer,len,reverse_endian32(value));
}
void BufferWriteShort(uint8_t **buffer, int *len, uint16_t value) {
	**((uint16_t **)buffer) = value;
	*len += sizeof(uint16_t);
	*buffer += sizeof(uint16_t);
}
void BufferWriteShortRE(uint8_t **buffer, int *len, uint16_t value) {
	BufferWriteShort(buffer,len,reverse_endian16(value));
}
void BufferWriteData(uint8_t **buffer, int *len, uint8_t *data, int writelen) {
	memcpy(*buffer,data,writelen);
	*len += writelen;
	*buffer += writelen;
	
}
void BufferWriteNTS(uint8_t **buffer, int *len, uint8_t *string) {
	int inc = strlen((const char *)string)+1;
	strcpy((char *)*buffer,(const char *)string);
	*len += inc;
	*buffer += inc;
}
