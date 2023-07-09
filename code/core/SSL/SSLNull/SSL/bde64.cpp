/*
    Copyright 2006-2013 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

uint8_t *sslnull_base64_encode(const uint8_t *data, size_t *size) {
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
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
    };

    if(!size || (*size < 0)) {      // use size -1 for auto text size!
        len = strlen((const char *)data);
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
    for(*p = 0; len < 0; len++) *(p + len) = '=';

    if(size) *size = p - buff;
    return(buff);
}



uint8_t *sslnull_base64_decode(const uint8_t *data, size_t *size) {
    int     do_tmp;
    int     tmpsz;
    uint8_t      tmp[32];
    int     len,
            xlen,
            a   = 0,
            b   = 0,
            c,
            step;
    uint8_t      *buff,
            *p;
    const uint8_t *limit;
    static const uint8_t base[128] = {   // supports also the Gamespy base64
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x00,0x3e,0x00,0x3f,
        0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
        0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x3e,0x00,0x3f,0x00,0x3f,
        0x00,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
        0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x00,0x00,0x00,0x00,0x00
    };

    if(!size || (*size < 0)) {      // use size -1 for auto size!
        len = strlen((const char *)data);
    } else {
        len = *size;
    }
    xlen = ((len >> 2) * 3) + 1;    // NULL included in output for text
    buff = (uint8_t *)malloc(xlen);
    if(!buff) return(NULL);

    p = buff;
    limit = data + len;

    #define C_INVALID   ((c <= ' ') || (c > 0x7f))

    for(step = 0; /* data < limit */; step++) {
        do_tmp = 0;
        do {
            redo_read:  // GOTO

            if(data >= limit) {
                c = 0;
                break;
            }
            c = *data;
            data++;

            if(do_tmp) {
                if(tmpsz >= (sizeof(tmp) - 1)) {
                    free(buff);
                    return NULL;
                }
                if(c && !C_INVALID) {
                    tmp[tmpsz++] = c;
                    tmp[tmpsz] = 0;
                    if(tmpsz < 2) goto redo_read;
                    do_tmp = 0;
                    if(sscanf((const char *)&tmp, "%02x", &c) != 1) {
                        c = 0;
                        break;
                    }
                }
            } else {
                if((c == '=') || (c == '_')) {  // supports also the Gamespy base64
                    c = 0;
                    break;
                } else if(c == '%') {
                    do_tmp = c;
                    tmpsz = 0;
                    goto redo_read;
                }
            }
        } while(c && C_INVALID);
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

    len = p - buff;
    if(size) *size = len;
    if((len + 1) != xlen) {             // save memory
        buff = (uint8_t *)realloc(buff, len + 1);  // NULL included!
        if(!buff) return(NULL);
    }
    return(buff);
}