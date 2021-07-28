/*
GSMSALG 0.3.3
by Luigi Auriemma
e-mail: aluigi@autistici.org
web:    aluigi.org


INTRODUCTION
============
With the name Gsmsalg I define the challenge-response algorithm needed
to query the master servers that use the Gamespy "secure" protocol (like
master.gamespy.com for example).
This algorithm is not only used for this type of query but also in other
situations like the so called "Gamespy Firewall Probe Packet" and the
master server hearbeat that is the challenge string sent by the master
servers of the games that use the Gamespy SDK when game servers want to
be included in the online servers list (UDP port 27900).


HOW TO USE
==========
The function needs 4 parameters:
- dst:     the destination buffer that will contain the calculated
           response. Its length is 4/3 of the challenge size so if the
           challenge is 6 bytes long, the response will be 8 bytes long
           plus the final NULL byte which is required (to be sure of the
           allocated space use 89 bytes or "((len * 4) / 3) + 3")
           if this parameter is NULL the function will allocate the
           memory for a new one automatically
- src:     the source buffer containing the challenge string received
           from the server.
- key:     the gamekey or any other text string used as algorithm's
           key, usually it is the gamekey but "might" be another thing
           in some cases. Each game has its unique Gamespy gamekey which
           are available here:
           http://aluigi.org/papers/gslist.cfg
- enctype: are supported 0 (plain-text used in old games, heartbeat
           challenge respond, enctypeX and more), 1 (Gamespy3D) and 2
           (old Gamespy Arcade or something else).

The return value is a pointer to the destination buffer.


EXAMPLE
=======
  #include "gsmsalg.h"

  char  *dest;
  dest = gsseckey(
    NULL,       // dest buffer, NULL for auto allocation
    "ABCDEF",   // the challenge received from the server
    "kbeafe",   // kbeafe of Doom 3 and enctype set to 0
    0);         // enctype 0


LICENSE
=======
    Copyright 2004,2005,2006,2007,2008 Luigi Auriemma

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

#include <stdlib.h>
#include <string.h>



unsigned char gsvalfunc(int reg) {
    if(reg < 26) return(reg + 'A');
    if(reg < 52) return(reg + 'G');
    if(reg < 62) return(reg - 4);
    if(reg == 62) return('+');
    if(reg == 63) return('/');
    return(0);
}



unsigned char *gsseckey(
  unsigned char *dst,
  const char *src,
  const unsigned char *key,
  int           enctype) {

    int             i,
                    size,
                    keysz;
    unsigned char   enctmp[256],
                    tmp[66],
                    x,
                    y,
                    z,
                    a,
                    b,
                    *p;
    static const unsigned char enctype1_data[] = /* pre-built */
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

    if(!dst) {
        dst = (unsigned char *)malloc(89);
        if(!dst) return(NULL);
    }
    size = strlen((const char *)src);
    if((size < 1) || (size > 65)) {
        dst[0] = 0;
        return(dst);
    }
    keysz = strlen((const char *)key);

    for(i = 0; i < 256; i++) {
        enctmp[i] = i;
    }

    a = 0;
    for(i = 0; i < 256; i++) {
        a += enctmp[i] + key[i % keysz];
        x = enctmp[a];
        enctmp[a] = enctmp[i];
        enctmp[i] = x;
    }

    a = 0;
    b = 0;
    for(i = 0; src[i]; i++) {
        a += src[i] + 1;
        x = enctmp[a];
        b += x;
        y = enctmp[b];
        enctmp[b] = x;
        enctmp[a] = y;
        tmp[i] = src[i] ^ enctmp[(x + y) & 0xff];
    }
    for(size = i; size % 3; size++) {
        tmp[size] = 0;
    }

    if(enctype == 1) {
        for(i = 0; i < size; i++) {
            tmp[i] = enctype1_data[tmp[i]];
        }
    } else if(enctype == 2) {
        for(i = 0; i < size; i++) {
            tmp[i] ^= key[i % keysz];
        }
    }

    p = dst;
    for(i = 0; i < size; i += 3) {
        x = tmp[i];
        y = tmp[i + 1];
        z = tmp[i + 2];
        *p++ = gsvalfunc(x >> 2);
        *p++ = gsvalfunc(((x & 3) << 4) | (y >> 4));
        *p++ = gsvalfunc(((y & 15) << 2) | (z >> 6));
        *p++ = gsvalfunc(z & 63);
    }
    *p = 0;

    return(dst);
}

