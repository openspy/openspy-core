#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int gslame(int num) {
    int     a,
            c;

    c = (num >> 16) & 0xffff;
    a = num & 0xffff;
    c *= 0x41a7;
    a *= 0x41a7;
    a += ((c & 0x7fff) << 16);
    if(a < 0) {
        a &= 0x7fffffff;
        a++;
    }
    a += (c >> 15);
    if(a < 0) {
        a &= 0x7fffffff;
        a++;
    }
    return(a);
}



int gspassenc(uint8_t *pass) {
    int     i,
            a,
            c,
            d,
            num,
            passlen;

    passlen = (int)strlen((const char *)pass);
    num = 0x79707367;   // "gspy"
    if(!num) {
        num = 1;
    } else {
        num &= 0x7fffffff;
    }

    for(i = 0; i < passlen; i++) {
        d = 0xff;
        c = 0;
        d -= c;
        if(d) {
            num = gslame(num);
            a = num % d;
            a += c;
        } else {
            a = c;
        }
        pass[i] ^= a;
    }
    return(passlen);
}



uint8_t *base64_encode(uint8_t *data, int *size) {    // Gamespy specific!!!
    int     len,
            a,
            b,
            c;
    uint8_t      *buff,
            *p;
    static const uint8_t base[64] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','[',']'
    };

    if(!size || (*size < 0)) {      // use size -1 for auto text size!
        len = (int)strlen((const char *)data);
    } else {
        len = *size;
    }
    buff = (uint8_t *)malloc(((len / 3) << 2) + 6);
    if(!buff) return(NULL);

    p = buff;
    do {
        a = data[0];
        b = data[1];
        c = data[2];
        *p++ = base[(a >> 2) & 63];
        *p++ = base[(((a &  3) << 4) | ((b >> 4) & 15)) & 63];
        *p++ = base[(((b & 15) << 2) | ((c >> 6) &  3)) & 63];
        *p++ = base[c & 63];
        data += 3;
        len  -= 3;
    } while(len > 0);
    for(*p = 0; len < 0; len++) *(p + len) = '_';

    if(size) *size = (int)(p - buff);
    return(buff);
}



uint8_t *base64_decode(uint8_t *data, int *size) {
    int     len,
            xlen,
            a   = 0,
            b   = 0,
            c,
            step;
    uint8_t      *buff,
            *limit,
            *p;
    static const uint8_t base[128] = {   // supports also the Gamespy base64
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x3f,
        0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
        0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x3e,0x00,0x3f,0x00,0x00,
        0x00,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
        0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x00,0x00,0x00,0x00,0x00
    };

    if(!size || (*size < 0)) {      // use size -1 for auto size!
        len = (int)strlen((const char *)data);
    } else {
        len = *size;
    }
    xlen = ((len >> 2) * 3) + 1;    // NULL included in output for text
    buff = (uint8_t *)malloc(xlen);
    if(!buff) return(NULL);

    p = buff;
    limit = data + len;

    for(step = 0; /* data < limit */; step++) {
        do {
            if(data >= limit) {
                c = 0;
                break;
            }
            c = *data;
            data++;
            if((c == '=') || (c == '_')) {  // supports also the Gamespy base64
                c = 0;
                break;
            }
        } while(c && ((c <= ' ') || (c > 0x7f)));
        if(!c) break;

        switch(step & 3) {
            case 0: {
                a = base[c];
                break;
            }
            case 1: {
                b = base[c];
                *p++ = (a << 2)        | (b >> 4);
                break;
            }
            case 2: {
                a = base[c];
                *p++ = ((b & 15) << 4) | (a >> 2);
                break;
            }
            case 3: {
                *p++ = ((a & 3) << 6)  | base[c];
                break;
            }
        }
    }
    *p = 0;

    len = (int)(p - buff);
    if(size) *size = len;
    if((len + 1) != xlen) {             // save memory
        buff = (uint8_t *)realloc(buff, len + 1);  // NULL included!
        if(!buff) return(NULL);
    }
    return(buff);
}
void gamespyxor(char *data, int len) {
    char      gamespy[] = "gamespy",
            *gs;

    for(gs = gamespy; len; len--, gs++, data++) {
        if(!*gs) gs = gamespy;
        *data ^= *gs;
    }
}
void gamespy3dxor(char *data, int len) {
    static const char gamespy[] = "GameSpy3D\0";
    char  *gs;

    gs = (char *)gamespy;
    while(len-- && len >= 0) {
	if(strncmp(data,"\\final\\",7) == 0)  {
		data+=7;
		len-=7;
		gs = (char *)gamespy;
		continue;
	}
        *data++ ^= *gs++;
        if(!*gs) gs = (char *)gamespy;
    }
}