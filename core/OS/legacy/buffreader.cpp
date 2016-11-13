#include "buffreader.h"
#include "helpers.h"
uint8_t BufferReadByte(uint8_t **buffer, int *len) {
	uint8_t byte = 0;
	if(*len >= sizeof(uint8_t)) {
		byte = *(*buffer);
		*buffer += sizeof(uint8_t);
		*len -= sizeof(uint8_t);
	}
	return byte;
}
uint32_t BufferReadInt(uint8_t **buffer, int *len) {
	uint32_t retint = 0;
	if(*len >= sizeof(uint32_t)) {
		retint = *((uint32_t *)(*buffer));
		*buffer += sizeof(uint32_t);
		*len -= sizeof(uint32_t);
	}
	return retint;
}
uint32_t BufferReadIntRE(uint8_t **buffer, int *len) {
	return reverse_endian32(BufferReadInt(buffer,len));
}
uint16_t BufferReadShort(uint8_t **buffer, int *len) {
	uint16_t retint = 0;
	if(*len >= sizeof(uint16_t)) {
		retint = *((uint16_t *)(*buffer));
		*buffer += sizeof(uint16_t);
		*len -= sizeof(uint16_t);
	}
	return retint;
}
uint16_t BufferReadShortRE(uint8_t **buffer, int *len) {
	return reverse_endian16(BufferReadShort(buffer,len));
}
uint8_t *BufferReadData(uint8_t **buffer, int *len, int readlen) {
	uint8_t *retbuff;
	if(readlen > *len) { //trying to read more than is available
		return NULL;
	}
	retbuff = (uint8_t *)malloc(readlen);
	if(retbuff == NULL) return NULL;
	return BufferReadData(buffer,len,retbuff,readlen);
}
uint8_t *BufferReadData(uint8_t **buffer, int *len, uint8_t *dst, int readlen) {
	memcpy((void *)dst,(void *)*buffer,readlen);
	*buffer += readlen;
	*len -= readlen;
	return dst;
}
uint8_t *BufferReadNTS(uint8_t **buffer, int *len) {
	uint8_t *retbuff;
	int readlen;
	uint8_t *nullbyte = (uint8_t *)find_first((char *)*buffer,0,*len);
	if(nullbyte == NULL) { //we didn't find a null byte so just stop
		return NULL;
	}
	readlen = nullbyte - *buffer;
	retbuff = (uint8_t *)malloc(readlen+1);
	if(retbuff == NULL) return NULL;
	memset(retbuff,0,readlen+1);
	BufferReadData(buffer,len,retbuff,readlen);
	*buffer += sizeof(uint8_t); //skip the null byte
	*len -= sizeof(uint8_t);
	return retbuff;
}
