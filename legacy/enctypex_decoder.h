#ifndef _ENCTYPEX_DECODER_H
#define _ENCTYPEX_DECODER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#define _JUST_STRUCTS
#define _NO_CPP
#include "../server/SBPeer.h"
#undef _NO_CPP
#undef _JUST_STRUCTS
typedef struct {
    unsigned char   encxkey[261];   // static key
    int             offset;         // everything decrypted till now (total)
    int             start;          // where starts the buffer (so how much big is the header), this is the only one you need to zero
} enctypex_data_t;
int enctypex_wrapper(unsigned char *key, unsigned char *validate, unsigned char *data, int size);
int enctypex_quick_encrypt(unsigned char *key, unsigned char *validate, unsigned char *data, int size);
unsigned char *enctypex_msname(unsigned char *gamename, unsigned char *retname);
unsigned char *enctypex_encoder(unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data);
unsigned char *enctypex_decoder(unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data);
unsigned char *enctypex_init(unsigned char *encxkey, unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data);
void enctypex_decoder_rand_validate(unsigned char *validate);
int enctypex_decoder_convert_to_ipport(unsigned char *data, int datalen, unsigned char *out, unsigned char *infobuff, int infobuff_size, int infobuff_offset);
int enctypex_data_cleaner(unsigned char *dst, unsigned char *src, int max);
void enctypex_funcx(unsigned char *encxkey, unsigned char *key, unsigned char *encxvalidate, unsigned char *data, int datalen);
int enctypex_func6e(unsigned char *encxkey, unsigned char *data, int len);
int enctypex_func6(unsigned char *encxkey, unsigned char *data, int len);
int enctypex_func7e(unsigned char *encxkey, unsigned char d);
int enctypex_func7(unsigned char *encxkey, unsigned char d);
void enctypex_func4(unsigned char *encxkey, unsigned char *id, int idlen);
int enctypex_func5(unsigned char *encxkey, int cnt, unsigned char *id, int idlen, int *n1, int *n2);
#endif
