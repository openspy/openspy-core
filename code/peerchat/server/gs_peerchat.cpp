/*

GS peerchat encryption/decryption algorithm 0.2a
by Luigi Auriemma
e-mail: aluigi@autistici.org
web:    aluigi.org


INTRODUCTION
============
This algorithm is used by the games that implement the Gamespy IRC
protocol (IRC server peerchat.gamespy.com)
I have written this code because the first right of the users is to
know what really happens on their computers and the Gamespy IRC
protocol uses an encryption method to hide the exchanged data.


HOW TO USE
==========
gs_peerchat_ctx
---------------
this is the structure that contains all the needed variables and buffers.
You must assign it to both client and server.

gs_peerchat_init
----------------
needs 3 arguments:
- the gs_peerchat_ctx structure
- the challenge of the host which refers the structure
- the gamekey of the game

gs_peerchat
-----------
needs 3 arguments:
- ctx:  the gs_peerchat_ctx structure
- data: the buffer containing the data to encrypt/decrypt
- size: the size of the data

Watch the source code of the toolz I have released on my website for any
doubt about the usage of this function:

  http://aluigi.org/papers.htm


LICENSE
=======
    Copyright 2004,2005,2006 Luigi Auriemma

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

#include "gs_peerchat.h"

void gs_peerchat_init(gs_peerchat_ctx *ctx, unsigned char *chall, unsigned char *gamekey) {
    unsigned char   challenge[16],
                    *l,
                    *l1,
                    *p,
                    *p1,
                    *crypt,
                    t,
                    t1;

    ctx->gs_peerchat_1 = 0;
    ctx->gs_peerchat_2 = 0;
    crypt              = ctx->gs_peerchat_crypt;

    p  = challenge;
    l  = challenge + 16;
    p1 = gamekey;
    do {
        if(!*p1) p1 = gamekey;
        *p++ = *chall++ ^ *p1++;    // avoid a memcpy(challenge, chall, 16);
    } while(p != l);

    t1 = 255;
    p1 = crypt;
    l1 = crypt + 256;
    do { *p1++ = t1--; } while(p1 != l1);

    t1++;       // means t1 = 0;
    p  = crypt;
    p1 = challenge;
    do {
        t1 += *p1 + *p;
        t = crypt[t1];
        crypt[t1] = *p;
        *p = t;
        p++;
        p1++;
        if(p1 == l) p1 = challenge;
    } while(p != l1);
}



void gs_peerchat(gs_peerchat_ctx *ctx, unsigned char *data, int size) {
    unsigned char   num1,
                    num2,
                    t,
                    *crypt;

    num1  = ctx->gs_peerchat_1;
    num2  = ctx->gs_peerchat_2;
    crypt = ctx->gs_peerchat_crypt;

    while(size--) {
        t = crypt[++num1];
        num2 += t;
        crypt[num1] = crypt[num2];
        crypt[num2] = t;
        t += crypt[num1];
        *data++ ^= crypt[t];
    }

    ctx->gs_peerchat_1 = num1;
    ctx->gs_peerchat_2 = num2;
}
