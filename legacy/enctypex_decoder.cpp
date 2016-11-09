/*
GS enctypeX servers list decoder/encoder 0.1.3a
by Luigi Auriemma
e-mail: aluigi@autistici.org
web:    aluigi.org

This is the algorithm used by ANY new and old game which contacts the Gamespy master server.
It has been written for being used in gslist so there are no explanations or comments here,
if you want to understand something take a look to gslist.c

    Copyright 2008,2009 Luigi Auriemma

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
#define _CRT_SECURE_NO_WARNINGS
#include "enctypex_decoder.h"


int enctypex_func5(unsigned char *encxkey, int cnt, unsigned char *id, int idlen, int *n1, int *n2) {
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
        *n1 = encxkey[*n1 & 0xff] + id[*n2];
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



void enctypex_func4(unsigned char *encxkey, unsigned char *id, int idlen) {
	int             i,
                    n1 = 0,
                    n2 = 0;
    unsigned char   t1,
                    t2;

    if(idlen < 1) return;

    for(i = 0; i < 256; i++) encxkey[i] = i;

    for(i = 255; i >= 0; i--) {
        t1 = enctypex_func5(encxkey, i, id, idlen, &n1, &n2);
        t2 = encxkey[i];
        encxkey[i] = encxkey[t1];
        encxkey[t1] = t2;
    }

    encxkey[256] = encxkey[1];
    encxkey[257] = encxkey[3];
    encxkey[258] = encxkey[5];
    encxkey[259] = encxkey[7];
    encxkey[260] = encxkey[n1 & 0xff];
}



int enctypex_func7(unsigned char *encxkey, unsigned char d) {
    unsigned char   a,
                    b,
                    c;

    a = encxkey[256];
    b = encxkey[257];
    c = encxkey[a];
    encxkey[256] = a + 1;
    encxkey[257] = b + c;
    a = encxkey[260];
    b = encxkey[257];
    b = encxkey[b];
    c = encxkey[a];
    encxkey[a] = b;
    a = encxkey[259];
    b = encxkey[257];
    a = encxkey[a];
    encxkey[b] = a;
    a = encxkey[256];
    b = encxkey[259];
    a = encxkey[a];
    encxkey[b] = a;
    a = encxkey[256];
    encxkey[a] = c;
    b = encxkey[258];
    a = encxkey[c];
    c = encxkey[259];
    b += a;
    encxkey[258] = b;
    a = b;
    c = encxkey[c];
    b = encxkey[257];
    b = encxkey[b];
    a = encxkey[a];
    c += b;
    b = encxkey[260];
    b = encxkey[b];
    c += b;
    b = encxkey[c];
    c = encxkey[256];
    c = encxkey[c];
    a += c;
    c = encxkey[b];
    b = encxkey[a];
    encxkey[260] = d;
    c ^= b ^ d;
    encxkey[259] = c;
    return(c);
}



int enctypex_func7e(unsigned char *encxkey, unsigned char d) {
    unsigned char   a,
                    b,
                    c;

    a = encxkey[256];
    b = encxkey[257];
    c = encxkey[a];
    encxkey[256] = a + 1;
    encxkey[257] = b + c;
    a = encxkey[260];
    b = encxkey[257];
    b = encxkey[b];
    c = encxkey[a];
    encxkey[a] = b;
    a = encxkey[259];
    b = encxkey[257];
    a = encxkey[a];
    encxkey[b] = a;
    a = encxkey[256];
    b = encxkey[259];
    a = encxkey[a];
    encxkey[b] = a;
    a = encxkey[256];
    encxkey[a] = c;
    b = encxkey[258];
    a = encxkey[c];
    c = encxkey[259];
    b += a;
    encxkey[258] = b;
    a = b;
    c = encxkey[c];
    b = encxkey[257];
    b = encxkey[b];
    a = encxkey[a];
    c += b;
    b = encxkey[260];
    b = encxkey[b];
    c += b;
    b = encxkey[c];
    c = encxkey[256];
    c = encxkey[c];
    a += c;
    c = encxkey[b];
    b = encxkey[a];
    c ^= b ^ d;
    encxkey[260] = c;   // encrypt
    encxkey[259] = d;   // encrypt
    return(c);
}



int enctypex_func6(unsigned char *encxkey, unsigned char *data, int len) {
    int     i;

    for(i = 0; i < len; i++) {
        data[i] = enctypex_func7(encxkey, data[i]);
    }
    return(len);
}



int enctypex_func6e(unsigned char *encxkey, unsigned char *data, int len) {
    int     i;

    for(i = 0; i < len; i++) {
        data[i] = enctypex_func7e(encxkey, data[i]);
    }
    return(len);
}



void enctypex_funcx(unsigned char *encxkey, unsigned char *key, unsigned char *encxvalidate, unsigned char *data, int datalen) {
    int     i,
            keylen;

    keylen = strlen((const char *)key);
    for(i = 0; i < datalen; i++) {
        encxvalidate[(key[i % keylen] * i) % LIST_CHALLENGE_LEN] ^= encxvalidate[i % LIST_CHALLENGE_LEN] ^ data[i];
    }
    enctypex_func4(encxkey, encxvalidate, LIST_CHALLENGE_LEN);
}



static int enctypex_data_cleaner_level = 2; // 0 = do nothing
                                            // 1 = colors
                                            // 2 = colors + strange chars
                                            // 3 = colors + strange chars + sql



int enctypex_data_cleaner(unsigned char *dst, unsigned char *src, int max) {
    static const unsigned char strange_chars[] = {
                    ' ','E',' ',',','f',',','.','t',' ','^','%','S','<','E',' ','Z',
                    ' ',' ','`','`','"','"','.','-','-','~','`','S','>','e',' ','Z',
                    'Y','Y','i','c','e','o','Y','I','S','`','c','a','<','-','-','E',
                    '-','`','+','2','3','`','u','P','-',',','1','`','>','%','%','%',
                    '?','A','A','A','A','A','A','A','C','E','E','E','E','I','I','I',
                    'I','D','N','O','O','O','O','O','x','0','U','U','U','U','Y','D',
                    'B','a','a','a','a','a','a','e','c','e','e','e','e','i','i','i',
                    'i','o','n','o','o','o','o','o','+','o','u','u','u','u','y','b',
                    'y' };
    unsigned char   c,
                    *p;

    if(!dst) return(0);
    if(dst != src) dst[0] = 0;  // the only change in 0.1.3a
    if(!src) return(0);

    if(max < 0) max = strlen((const char *)src);

    for(p = dst; (c = *src) && (max > 0); src++, max--) {
        if(c == '\\') {                     // avoids the backslash delimiter
            *p++ = '/';
            continue;
        }

        if(enctypex_data_cleaner_level >= 1) {
            if(c == '^') {                  // Quake 3 colors
                //if(src[1] == 'x') {         // ^x112233 (I don't remember the game which used this format)
                    //src += 7;
                    //max -= 7;
                //} else
                if(isdigit(src[1]) || islower(src[1])) { // ^0-^9, ^a-^z... a good compromise
                    src++;
                    max--;
                } else {
                    *p++ = c;
                }
                continue;
            }
            if(c == 0x1b) {                 // Unreal colors
                src += 3;
                max -= 3;
                continue;
            }
            if(c < ' ') {                   // other colors
                continue;
            }
        }

        if(enctypex_data_cleaner_level >= 2) {
            if(c >= 0x7f) c = strange_chars[c - 0x7f];
        }

        if(enctypex_data_cleaner_level >= 3) {
            switch(c) {                     // html/SQL injection (paranoid mode)
                case '\'':
                case '\"':
                case '&':
                case '^':
                case '?':
                case '{':
                case '}':
                case '(':
                case ')':
                case '[':
                case ']':
                case '-':
                case ';':
                case '~':
                case '|':
                case '$':
                case '!':
                case '<':
                case '>':
                case '*':
                case '%':
                case ',': c = '.';  break;
                default: break;
            }
        }

        if((c == '\r') || (c == '\n')) {    // no new line
            continue;
        }
        *p++ = c;
    }
    *p = 0;
    return(p - dst);
}



    // function not related to the algorithm, I have created it only for a quick handling of the received data
    // very quick explanation:
    // - if you use out it will be considered as an output buffer where placing all the IP and ports of the servers in the classical format: 4 bytes for IP and 2 for the port
    // - if you don't use out the function will return a non-zero value if you have received all the data from the master server
    // - if you use infobuff it will be considered as an output buffer where placing all the informations of one server at time in the format "IP:port \parameter1\value1\...\parameterN\valueN"
    // - infobuff_size is used to avoid to write more data than how much supported by infobuff
    // - infobuff_offset instead is used for quickly handling the next servers for infobuff because, as just said, the function handles only one server at time
    //   infobuff_offset is just the offset of the server to handle in our enctypex buffer, the function returns the offset to the next one or a value zero or -1 if there are no other hosts
    // data and out can't be the same buffer because in some games like AA the gamespy master server returns
    // 5 bytes for each IP/port and so there is the risk of overwriting the data to handle, that's why I use
    // an output buffer which is at least "(datalen / 5) * 6" bytes long
int enctypex_decoder_convert_to_ipport(unsigned char *data, int datalen, unsigned char *out, unsigned char *infobuff, int infobuff_size, int infobuff_offset) {
#define enctypex_infobuff_check(X) \
    if(infobuff) { \
        if((int)(infobuff_size - infobuff_len) <= (int)(X)) { \
            infobuff_size = 0; \
        } else

    typedef struct {
        unsigned char   type;
        unsigned char   *name;
    } par_t;

    int             i,
                    len,
                    pars    = 0,    // pars and vals are used for making the function
                    vals    = 0,    // thread-safe when infobuff is not used
                    infobuff_len = 0;
    unsigned char   tmpip[6],
                    port[2],
                    t,
                    *p,
                    *o,
                    *l;
    static const int    use_parval = 1; // par and val are required, so this bool is useless
    static unsigned char    // this function is not thread-safe if you use it for retrieving the extra data (infobuff)
                    parz    = 0,
                    valz    = 0,
                    **val   = NULL;
    static par_t    *par    = NULL; // par[255] and *val[255] was good too

    if(!data) return(0);
    if(datalen < 6) return(0);  // covers the 6 bytes of IP:port
    o = out;
    p = data;
    l = data + datalen;

    p += 4;         // your IP
    port[0] = *p++; // the most used port
    port[1] = *p++;
    if((port[0] == 0xff) && (port[1] == 0xff)) {
        return(-1); // error message from the server
    }

    if(infobuff && infobuff_offset) {   // restore the data
        p = data + infobuff_offset;
    } else {
        if(p < l) {
            pars = *p++;
            if(use_parval) {  // save the static data
                parz = pars;
                par  = (par_t *)realloc(par, sizeof(par_t) * parz);
            }
            for(i = 0; (i < pars) && (p < l); i++) {
                t = *p++;
                if(use_parval) {
                    par[i].type = t;
                    par[i].name = p;
                }
                p += strlen((const char *)p) + 1;
            }
        }
        if(p < l) {
            vals = *p++;
            if(use_parval) {  // save the static data
                valz = vals;
                *val  = (unsigned char *)realloc(val, sizeof(unsigned char *) * valz);
            }
            for(i = 0; (i < vals) && (p < l); i++) {
                if(use_parval) val[i] = p;
                p += strlen((const char *)p) + 1;
            }
        }
    }

    if(use_parval) {
        pars = parz;
        vals = valz;
    }
    if(infobuff && (infobuff_size > 0)) {
        infobuff[0] = 0;
    }

    while(p < l) {
        t = *p++;
        if(!t && !memcmp(p, "\xff\xff\xff\xff", 4)) {
            if(!out) o = out - 1;   // so the return is not 0 and means that we have reached the end
            break;
        }
        len = 5;
        if(t & 0x02) len = 9;
        if(t & 0x08) len += 4;
        if(t & 0x10) len += 2;
        if(t & 0x20) len += 2;

        tmpip[0] = p[0];
        tmpip[1] = p[1];
        tmpip[2] = p[2];
        tmpip[3] = p[3];
        if((len < 6) || !(t & 0x10)) {
            tmpip[4] = port[0];
            tmpip[5] = port[1];
        } else {
            tmpip[4] = p[4];
            tmpip[5] = p[5];
        }

        if(out) {
            memcpy(o, tmpip, 6);
            o += 6;
        }
        enctypex_infobuff_check(22) {
            infobuff_len = sprintf((char *)infobuff,
                "%u.%u.%u.%u:%hu ",
                tmpip[0], tmpip[1], tmpip[2], tmpip[3],
                (unsigned short)((tmpip[4] << 8) | tmpip[5]));
        }}

        p += len - 1;   // the value in len is no longer used from this point
        if(t & 0x40) {
            for(i = 0; (i < pars) && (p < l); i++) {
                enctypex_infobuff_check(1 + strlen((const char *)par[i].name) + 1) {
                    infobuff[infobuff_len++] = '\\';
                    infobuff_len += enctypex_data_cleaner(infobuff + infobuff_len, par[i].name, -1);
                    infobuff[infobuff_len++] = '\\';
                    infobuff[infobuff_len]   = 0;
                }}
                t = *p++;

                if(use_parval) {
                    if(!par[i].type) {  // string
                        if(t == 0xff) { // inline string
                            enctypex_infobuff_check(strlen((const char *)p)) {
                                infobuff_len += enctypex_data_cleaner(infobuff + infobuff_len, p, -1);
                            }}
                            p += strlen((const char *)p) + 1;
                        } else {        // fixed string
                            if(t < vals) {
                                enctypex_infobuff_check(strlen((const char *)val[t])) {
                                    infobuff_len += enctypex_data_cleaner(infobuff + infobuff_len, val[t], -1);
                                }}
                            }
                        }
                    } else {            // number (-128 to 127)
                        enctypex_infobuff_check(5) {
                            infobuff_len += sprintf((char *)(infobuff + infobuff_len), "%d", (signed char)t);
                        }}
                    }
                }
            }
        }
        if(infobuff) {  // do NOT touch par/val, I use realloc
            return(p - data);
        }
    }

    if((out == data) && ((o - out) > (p - data))) { // I need to remember this
        fprintf(stderr, "\nError: input and output buffer are the same and there is not enough space\n");
        exit(1);
    }
    if(infobuff) {      // do NOT touch par/val, I use realloc
        parz = 0;
        valz = 0;
        return(-1);
    }
    return(o - out);
}



void enctypex_decoder_rand_validate(unsigned char *validate) {
    int     i,
            rnd;

    rnd = ~time(NULL);
    for(i = 0; i < 8; i++) {
        do {
            rnd = ((rnd * 0x343FD) + 0x269EC3) & 0x7f;
        } while((rnd < 0x21) || (rnd >= 0x7f));
        validate[i] = rnd;
    }
    validate[i] = 0;
}



unsigned char *enctypex_init(unsigned char *encxkey, unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data) {
    int             a,
                    b;
    unsigned char   encxvalidate[LIST_CHALLENGE_LEN];

    if(*datalen < 1) return(NULL);
    a = (data[0] ^ 0xec) + 2;
    if(*datalen < a) return(NULL);
    b = data[a - 1] ^ 0xea;
    if(*datalen < (a + b)) return(NULL);
    memcpy(encxvalidate, validate, LIST_CHALLENGE_LEN);
    enctypex_funcx(encxkey, key, encxvalidate, data + a, b);
    a += b;
    if(!enctypex_data) {
        data     += a;
        *datalen -= a;  // datalen is untouched in stream mode!!!
    } else {
        enctypex_data->offset = a;
        enctypex_data->start  = a;
    }
    return(data);
}



unsigned char *enctypex_decoder(unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data) {
    unsigned char   encxkeyb[261],
                    *encxkey;

    encxkey = enctypex_data ? enctypex_data->encxkey : encxkeyb;

    if(!enctypex_data || (enctypex_data && !enctypex_data->start)) {
        data = enctypex_init(encxkey, key, validate, data, datalen, enctypex_data);
        if(!data) return(NULL);
    }
    if(!enctypex_data) {
        enctypex_func6(encxkey, data, *datalen);
        return(data);
    } else if(enctypex_data && enctypex_data->start) {
        enctypex_data->offset += enctypex_func6(encxkey, data + enctypex_data->offset, *datalen - enctypex_data->offset);
        return(data + enctypex_data->start);
    }
    return(NULL);
}



// exactly as above but with enctypex_func6e instead of enctypex_func6
unsigned char *enctypex_encoder(unsigned char *key, unsigned char *validate, unsigned char *data, int *datalen, enctypex_data_t *enctypex_data) {
    unsigned char   encxkeyb[261],
                    *encxkey;

    encxkey = enctypex_data ? enctypex_data->encxkey : encxkeyb;

    if(!enctypex_data || (enctypex_data && !enctypex_data->start)) {
        data = enctypex_init(encxkey, key, validate, data, datalen, enctypex_data);
        if(!data) return(NULL);
    }
    if(!enctypex_data) {
        enctypex_func6e(encxkey, data, *datalen);
        return(data);
    } else if(enctypex_data && enctypex_data->start) {
        enctypex_data->offset += enctypex_func6e(encxkey, data + enctypex_data->offset, *datalen - enctypex_data->offset);
        return(data + enctypex_data->start);
    }
    return(NULL);
}



unsigned char *enctypex_msname(unsigned char *gamename, unsigned char *retname) {
    static unsigned char    msname[256];
    unsigned    i,
                c,
                server_num;

    server_num = 0;
    for(i = 0; gamename[i]; i++) {
        c = tolower(gamename[i]);
        server_num = c - (server_num * 0x63306ce7);
    }
    server_num %= 20;

    if(retname) {
        snprintf((char *)retname, 256, "%s.ms%d.gamespy.com", gamename, server_num);
        return(retname);
    }
    snprintf((char *)msname, sizeof(msname), "%s.ms%d.gamespy.com", gamename, server_num);
    return(msname);
}



int enctypex_wrapper(unsigned char *key, unsigned char *validate, unsigned char *data, int size) {
    unsigned char   *p;

    if(!key || !validate || !data || (size < 0)) return(0);

    p = enctypex_decoder(key, validate, data, &size, NULL);
    memmove(data, p, size);
    return(size);
}



// data must be enough big to include the 23 bytes header, remember it: data = realloc(data, size + 23);
int enctypex_quick_encrypt(unsigned char *key, unsigned char *validate, unsigned char *data, int size) {
    int             i,
                    rnd,
                    tmpsize,
                    keylen,
                    vallen;
    unsigned char   tmp[23];

    if(!key || !validate || !data || (size < 0)) return(0);

    keylen = strlen((const char *)key);   // only for giving a certain randomness, so useless
    vallen = strlen((const char *)validate);
    rnd = ~time(NULL);
    for(i = 0; i < sizeof(tmp); i++) {
        rnd = (rnd * 0x343FD) + 0x269EC3;
        tmp[i] = rnd ^ key[i % keylen] ^ validate[i % vallen];
    }
    tmp[0] = 0xeb;  // 7
    tmp[1] = 0x00;
    tmp[2] = 0x00;
    tmp[8] = 0xe4;  // 14

    for(i = size - 1; i >= 0; i--) {
        data[sizeof(tmp) + i] = data[i];
    }
    memcpy(data, tmp, sizeof(tmp));
    size += sizeof(tmp);

    tmpsize = size;
    enctypex_encoder(key, validate, data, &tmpsize, NULL);
    return(size);
}

