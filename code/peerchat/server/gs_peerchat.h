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



typedef struct {
    unsigned char   gs_peerchat_1;
    unsigned char   gs_peerchat_2;
    unsigned char   gs_peerchat_crypt[256];
} gs_peerchat_ctx;



void gs_peerchat_init(gs_peerchat_ctx *ctx, unsigned char *chall, unsigned char *gamekey);
void gs_peerchat(gs_peerchat_ctx *ctx, unsigned char *data, int size);
