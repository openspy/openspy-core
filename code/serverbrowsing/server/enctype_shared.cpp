/*
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

void encshare2(unsigned int *tbuff, unsigned int *tbuffp, int len) {
    unsigned int    t1,
                    t2,
                    t3,
                    t4,
                    t5,
                    *limit,
                    *p;

    t2 = tbuff[304];
    t1 = tbuff[305];
    t3 = tbuff[306];
    t5 = tbuff[307];
    limit = tbuffp + len;
    while(tbuffp < limit) {
        p = tbuff + t2 + 272;
        while(t5 < 65536) {
            t1 += t5;
            p++;
            t3 += t1;
            t1 += t3;
            p[-17] = t1;
            p[-1] = t3;
            t4 = (t3 << 24) | (t3 >> 8);
            p[15] = t5;
            t5 <<= 1;
            t2++;
            t1 ^= tbuff[t1 & 0xff];
            t4 ^= tbuff[t4 & 0xff];
            t3 = (t4 << 24) | (t4 >> 8);
            t4 = (t1 >> 24) | (t1 << 8);
            t4 ^= tbuff[t4 & 0xff];
            t3 ^= tbuff[t3 & 0xff];
            t1 = (t4 >> 24) | (t4 << 8);
        }
        t3 ^= t1;
        *tbuffp++ = t3;
        t2--;
        t1 = tbuff[t2 + 256];
        t5 = tbuff[t2 + 272];
        t1 = ~t1;
        t3 = (t1 << 24) | (t1 >> 8);
        t3 ^= tbuff[t3 & 0xff];
        t5 ^= tbuff[t5 & 0xff];
        t1 = (t3 << 24) | (t3 >> 8);
        t4 = (t5 >> 24) | (t5 << 8);
        t1 ^= tbuff[t1 & 0xff];
        t4 ^= tbuff[t4 & 0xff];
        t3 = (t4 >> 24) | (t4 << 8);
        t5 = (tbuff[t2 + 288] << 1) + 1;
    }
    tbuff[304] = t2;
    tbuff[305] = t1;
    tbuff[306] = t3;
    tbuff[307] = t5;
}



void encshare1(unsigned int *tbuff, unsigned char *datap, int len) {
    unsigned char   *p,
                    *s;

    p = s = (unsigned char *)(tbuff + 309);
    encshare2(tbuff, (unsigned int *)p, 16);

    while(len--) {
        if((p - s) == 63) {
            p = s;
            encshare2(tbuff, (unsigned int *)p, 16);
        }
        *datap ^= *p;
        datap++;
        p++;
    }
}



void encshare3(unsigned int *data, int n1, int n2) {
    unsigned int    t1,
                    t2,
                    t3,
                    t4;
    int             i;

    t2 = n1;
    t1 = 0;
    t4 = 1;
    data[304] = 0;
    for(i = 32768; i; i >>= 1) {
        t2 += t4;
        t1 += t2;
        t2 += t1;
        if(n2 & i) {
            t2 = ~t2;
            t4 = (t4 << 1) + 1;
            t3 = (t2 << 24) | (t2 >> 8);
            t3 ^= data[t3 & 0xff];
            t1 ^= data[t1 & 0xff];
            t2 = (t3 << 24) | (t3 >> 8);
            t3 = (t1 >> 24) | (t1 << 8);
            t2 ^= data[t2 & 0xff];
            t3 ^= data[t3 & 0xff];
            t1 = (t3 >> 24) | (t3 << 8);
        } else {
            data[data[304] + 256] = t2;
            data[data[304] + 272] = t1;
            data[data[304] + 288] = t4;
            data[304]++;
            t3 = (t1 << 24) | (t1 >> 8);
            t2 ^= data[t2 & 0xff];
            t3 ^= data[t3 & 0xff];
            t1 = (t3 << 24) | (t3 >> 8);
            t3 = (t2 >> 24) | (t2 << 8);
            t3 ^= data[t3 & 0xff];
            t1 ^= data[t1 & 0xff];
            t2 = (t3 >> 24) | (t3 << 8);
            t4 <<= 1;
        }
    }
    data[305] = t2;
    data[306] = t1;
    data[307] = t4;
    data[308] = n1;
    //    t1 ^= t2;
}



void encshare4(unsigned char *src, int size, unsigned int *dest) {
    unsigned int    tmp;
    int             i;
    unsigned char   pos,
                    x,
                    y;

    for(i = 0; i < 256; i++) dest[i] = 0;

    for(y = 0; y < 4; y++) {
        for(i = 0; i < 256; i++) {
            dest[i] = (dest[i] << 8) + i;
        }

        for(pos = y, x = 0; x < 2; x++) {
            for(i = 0; i < 256; i++) {
                tmp = dest[i];
                pos += tmp + src[i % size];
                dest[i] = dest[pos];
                dest[pos] = tmp;
            }
        }
    }

    for(i = 0; i < 256; i++) dest[i] ^= i;

    encshare3(dest, 0, 0);
}


