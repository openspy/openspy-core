#include <stdlib.h>
#include <string.h>
#ifndef _GSMALG_INC
#define _GSMALG_INC
unsigned char gsvalfunc(int reg);
unsigned char *gsseckey(unsigned char *dst, unsigned char *src, const unsigned char *key,  int enctype);
#endif