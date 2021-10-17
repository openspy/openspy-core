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

A simpler way to use the enctype1_function is just using:

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
#include <stdio.h>


void encshare1(unsigned int *tbuff, unsigned char *datap, int len);
void encshare4(unsigned char *src, int size, unsigned int *dest);

void show_dump(unsigned char *data, unsigned int len, FILE *stream);


unsigned char *enctype1_decoder(unsigned char *id, unsigned char *, int *);
void enctype1_func1(unsigned char *, int);
void enctype1_func2(unsigned char *, int, unsigned char *);
void enctype1_func3(unsigned char *, int, unsigned char *);
void enctype1_func6(unsigned char *data, int len, unsigned char *enc1key);
int  enctype1_func7(int, unsigned char *);
void enctype1_func4(unsigned char *, int, unsigned char *);
int  enctype1_func5(int, unsigned char *, int, int *, int *, unsigned char *);
void enctype1_func8(unsigned char *, int, const unsigned char *);


    // 00406720
void enctype1_func2(unsigned char *data, int size, unsigned char *crypt) {
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
void enctype1_func3(unsigned char *data, int len, unsigned char *buff) {
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
void enctype1_func4(unsigned char *id, int idlen, unsigned char *enc1key) {
	int             i,
                    n1 = 0,
                    n2 = 0;
    unsigned char   t1,
                    t2;

    if(idlen < 1) return;

    for(i = 0; i < 256; i++) enc1key[i] = i;

    for(i = 255; i >= 0; i--) {
        t1 = enctype1_func5(i, id, idlen, &n1, &n2, enc1key);
        t2 = enc1key[i];
        enc1key[i] = enc1key[t1];
        enc1key[t1] = t2;
    }

    enc1key[256] = enc1key[1];
    enc1key[257] = enc1key[3];
    enc1key[258] = enc1key[5];
    enc1key[259] = enc1key[7];
    enc1key[260] = enc1key[n1 & 0xff];
}



    // 0048D910
int enctype1_func5(int cnt, unsigned char *id, int idlen, int *n1, int *n2, unsigned char *enc1key) {
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
        *n1 = enc1key[*n1 & 0xff] + id[*n2];
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
void enctype1_func6(unsigned char *data, int len, unsigned char *enc1key) {
    while(len--) {
        *data = enctype1_func7(*data, enc1key);
        data++;
    }
}



    // 0048DB10
int enctype1_func7(int len, unsigned char *enc1key) {
    unsigned char   a,
                    b,
                    c;

    a = enc1key[256];
    b = enc1key[257];
    c = enc1key[a];
    enc1key[256] = a + 1;
    enc1key[257] = b + c;
    a = enc1key[260];
    b = enc1key[257];
    b = enc1key[b];
    c = enc1key[a];
    enc1key[a] = b;
    a = enc1key[259];
    b = enc1key[257];
    a = enc1key[a];
    enc1key[b] = a;
    a = enc1key[256];
    b = enc1key[259];
    a = enc1key[a];
    enc1key[b] = a;
    a = enc1key[256];
    enc1key[a] = c;
    b = enc1key[258];
    a = enc1key[c];
    c = enc1key[259];
    b = b + a;
    enc1key[258] = b;
    a = b;
    c = enc1key[c];
    b = enc1key[257];
    b = enc1key[b];
    a = enc1key[a];
    c += b;
    b = enc1key[260];
    b = enc1key[b];
    c += b;
    b = enc1key[c];
    c = enc1key[256];
    c = enc1key[c];
    a += c;
    c = enc1key[b];
    b = enc1key[a];
    a = len;
    c ^= b;
    enc1key[260] = a;
    c ^= a;
    enc1key[259] = c;
    return(c);
}



    // 004294B0
void enctype1_func8(unsigned char *data, int len, const unsigned char *enctype1_data) {
    while(len--) {
        *data = enctype1_data[*data];
        data++;
    }
}