#ifndef _OS_GAMESPY_H
#define _OS_GAMESPY_H
	int gslame(int num);
	int gspassenc(uint8_t *pass);
	uint8_t *base64_encode(uint8_t *data, int *size);
	uint8_t *base64_decode(uint8_t *data, int *size);
	void gamespyxor(char *data, int len);
	void gamespy3dxor(char *data, int len);
#endif //_OS_GAMESPY_H