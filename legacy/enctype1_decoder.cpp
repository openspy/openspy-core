/*

GS enctype1 servers list decoder 0.1a
by Luigi Auriemma
e-mail: aluigi@autistici.org
web:    aluigi.org


INTRODUCTION
============
This is the algorithm used to decrypt the data sent by the Gamespy
master server (or any other compatible server) using the enctype 1
method.


USAGE
=====
Add the following prototype at the beginning of your code:

  unsigned char *enctype1_decoder(unsigned char *, unsigned char *, int *);

save the \secure\ value in a small buffer (because gsmsalg modifies this
value or for any other reason) and then use:

        pointer = enctype1_decoder(
            secure,         // the \secure\ value
            buffer,         // all the data received from the master server
            &buffer_len);   // the size of the master server

The return value is a pointer to the decrypted zone of buffer and
buffer_len is modified with the size of the decrypted data.

A simpler way to use the function is just using:

  len = enctype1_wrapper(key, data, data_len);


THANX TO
========
REC (http://www.backerstreet.com/rec/rec.htm) which has helped me in many
parts of the code.


LICENSE
=======
    Copyright 2005,2006,2007,2008 Luigi Auriemma

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

    http://www.gnu.org/licenses/gpl.txt

*/

#include <string.h>

#include "enctype1_decoder.h"

const unsigned char enctype1_cryptdata[256 + 1] = /* pre-built */
        "\x01\xba\xfa\xb2\x51\x00\x54\x80\x75\x16\x8e\x8e\x02\x08\x36\xa5"
        "\x2d\x05\x0d\x16\x52\x07\xb4\x22\x8c\xe9\x09\xd6\xb9\x26\x00\x04"
        "\x06\x05\x00\x13\x18\xc4\x1e\x5b\x1d\x76\x74\xfc\x50\x51\x06\x16"
        "\x00\x51\x28\x00\x04\x0a\x29\x78\x51\x00\x01\x11\x52\x16\x06\x4a"
        "\x20\x84\x01\xa2\x1e\x16\x47\x16\x32\x51\x9a\xc4\x03\x2a\x73\xe1"
        "\x2d\x4f\x18\x4b\x93\x4c\x0f\x39\x0a\x00\x04\xc0\x12\x0c\x9a\x5e"
        "\x02\xb3\x18\xb8\x07\x0c\xcd\x21\x05\xc0\xa9\x41\x43\x04\x3c\x52"
        "\x75\xec\x98\x80\x1d\x08\x02\x1d\x58\x84\x01\x4e\x3b\x6a\x53\x7a"
        "\x55\x56\x57\x1e\x7f\xec\xb8\xad\x00\x70\x1f\x82\xd8\xfc\x97\x8b"
        "\xf0\x83\xfe\x0e\x76\x03\xbe\x39\x29\x77\x30\xe0\x2b\xff\xb7\x9e"
        "\x01\x04\xf8\x01\x0e\xe8\x53\xff\x94\x0c\xb2\x45\x9e\x0a\xc7\x06"
        "\x18\x01\x64\xb0\x03\x98\x01\xeb\x02\xb0\x01\xb4\x12\x49\x07\x1f"
        "\x5f\x5e\x5d\xa0\x4f\x5b\xa0\x5a\x59\x58\xcf\x52\x54\xd0\xb8\x34"
        "\x02\xfc\x0e\x42\x29\xb8\xda\x00\xba\xb1\xf0\x12\xfd\x23\xae\xb6"
        "\x45\xa9\xbb\x06\xb8\x88\x14\x24\xa9\x00\x14\xcb\x24\x12\xae\xcc"
        "\x57\x56\xee\xfd\x08\x30\xd9\xfd\x8b\x3e\x0a\x84\x46\xfa\x77\xb8";

    // 0048DAA0
void func1(unsigned char *id, int idlen, enctype1_data *cryptkey) {
    if(id && idlen) func4(id, idlen, cryptkey);
}



    // 00406720
void func2(unsigned char *data, int size, unsigned char *crypt) {
    unsigned char   n1,
                    n2,
                    t;

    n1 = crypt[256];
    n2 = crypt[257];
    while(size--) {
        t = crypt[++n1];
        n2 += t;
        crypt[n1] = crypt[n2];
        crypt[n2] = t;
        t += crypt[n1];
        *data++ ^= crypt[t];
    }
    crypt[256] = n1;
    crypt[257] = n2;
}



    // 00406680
void func3(unsigned char *data, int len, unsigned char *buff) {
    int             i;
    unsigned char   pos = 0,
                    tmp,
                    rev = 0xff;

    for(i = 0; i < 256; i++) {
        buff[i] = rev--;
    }

    buff[256] = 0;
    buff[257] = 0;
    for(i = 0; i < 256; i++) {
        tmp = buff[i];
        pos += data[i % len] + tmp;
        buff[i] = buff[pos];
        buff[pos] = tmp;
    }
}



    // 0048D9B0
void func4(unsigned char *id, int idlen, enctype1_data *cryptkey) {
	int             i,
                    n1 = 0,
                    n2 = 0;
    unsigned char   t1,
                    t2;

    if(idlen < 1) return;

    for(i = 0; i < 256; i++) cryptkey->enc1key[i] = i;

    for(i = 255; i >= 0; i--) {
        t1 = func5(i, id, idlen, &n1, &n2, cryptkey);
        t2 = cryptkey->enc1key[i];
        cryptkey->enc1key[i] = cryptkey->enc1key[t1];
        cryptkey->enc1key[t1] = t2;
    }

    cryptkey->enc1key[256] = cryptkey->enc1key[1];
    cryptkey->enc1key[257] = cryptkey->enc1key[3];
    cryptkey->enc1key[258] = cryptkey->enc1key[5];
    cryptkey->enc1key[259] = cryptkey->enc1key[7];
    cryptkey->enc1key[260] = cryptkey->enc1key[n1 & 0xff];
}



    // 0048D910
int func5(int cnt, unsigned char *id, int idlen, int *n1, int *n2, enctype1_data *cryptkey) {
    int     i,
            tmp,
            mask = 1;

    if(!cnt) return(0);
    if(cnt > 1) {
        do {
            mask = (mask << 1) + 1;
        } while(mask < cnt);
    }

    i = 0;
    do {
        *n1 = cryptkey->enc1key[*n1 & 0xff] + id[*n2];
        (*n2)++;
        if(*n2 >= idlen) {
            *n2 = 0;
            *n1 += idlen;
        }
        tmp = *n1 & mask;
        if(++i > 11) tmp %= cnt;
    } while(tmp > cnt);

    return(tmp);
}



    // 0048DC20
void func6(unsigned char *data, int len, enctype1_data *cryptkey) {
    while(len--) {
        *data = func7(*data,cryptkey);
        data++;
    }
}



    // 0048DB10
int func7(int len, enctype1_data *cryptkey) {
    unsigned char   a,
                    b,
                    c;

    a = cryptkey->enc1key[256];
    b = cryptkey->enc1key[257];
    c = cryptkey->enc1key[a];
    cryptkey->enc1key[256] = a + 1;
    cryptkey->enc1key[257] = b + c;
    a = cryptkey->enc1key[260];
    b = cryptkey->enc1key[257];
    b = cryptkey->enc1key[b];
    c = cryptkey->enc1key[a];
    cryptkey->enc1key[a] = b;
    a = cryptkey->enc1key[259];
    b = cryptkey->enc1key[257];
    a = cryptkey->enc1key[a];
    cryptkey->enc1key[b] = a;
    a = cryptkey->enc1key[256];
    b = cryptkey->enc1key[259];
    a = cryptkey->enc1key[a];
    cryptkey->enc1key[b] = a;
    a = cryptkey->enc1key[256];
    cryptkey->enc1key[a] = c;
    b = cryptkey->enc1key[258];
    a = cryptkey->enc1key[c];
    c = cryptkey->enc1key[259];
    b = b + a;
    cryptkey->enc1key[258] = b;
    a = b;
    c = cryptkey->enc1key[c];
    b = cryptkey->enc1key[257];
    b = cryptkey->enc1key[b];
    a = cryptkey->enc1key[a];
    c += b;
    b = cryptkey->enc1key[260];
    b = cryptkey->enc1key[b];
    c += b;
    b = cryptkey->enc1key[c];
    c = cryptkey->enc1key[256];
    c = cryptkey->enc1key[c];
    a += c;
    c = cryptkey->enc1key[b];
    b = cryptkey->enc1key[a];
    a = len;
    c ^= b;
    cryptkey->enc1key[260] = a;
    c ^= a;
    cryptkey->enc1key[259] = c;
    return(c);
}



    // 004294B0
void func8(unsigned char *data, int len, const unsigned char *enctype1_data) {
    while(len--) {
        *data = enctype1_data[*data];
        data++;
    }
}




