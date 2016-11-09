#define _CRT_SECURE_NO_WARNINGS
#include "helpers.h"

int delimit(char *data) {
    char      *p;
    for(p = data; *p && (*p != '\r') && (*p != '\n'); p++);
    *p = 0;
    return(p - data);
}

char *get_irc_command(char *buff, int *cmd) {
    char      *p,
            *c;

    *cmd = 0;
    p = strchr(buff, ' ');
    if(!p) return(buff);
    p++;

    *cmd = atoi(p);
    if(!*cmd) return(buff);

    c = strchr(p, ' ');
    if(!c) return(p);
    return(c + 1);
}

void strip(char *buff, char delim) {
	find_and_replace(buff,delim,0);
}

void find_and_replace(char *buff, char delim, int ch) {
	for(int i=0;i<strlen(buff);i++) {
		if(buff[i]==delim) {
			buff[i]=ch;
		}
	}
}


char *find_and_cut(char *buff, int num, char delim) {
	char *data;
	int i;
	for(data = buff, i = 0; i < num; i++, data++) {
		data = strchr(data, delim);
		if(!data) {
			return NULL;
		}
	}
	strip(data,delim);
	return data;
}

int find_end(char *buff, int len) {
	for(int i=0;i<len;i++) {
		if(buff[i]=='\r'||buff[i]=='\n') {
			if(!(i+1 > len)) {
				if(buff[i+1] == '\n'||buff[i+1] == '\r') {
					return i+2;
				}
			}
			return i+1;
		}
	}
	return -1;
}
char *find_last(char *buff, char delim, int len) {
	char *last=NULL;
	for(int i=0;i<len;i++) {
		if(buff[i]==delim) {
			last=buff+i;
		}
	}
	return last;
}
char *find_first(char *buff, char delim, int len) {
	for(int i=0;i<len;i++) {
		if(buff[i]==delim) {
			return buff+i;
		}
	}
	return NULL;
}
char *find_str(char *buff, int num, char ch) {
	char *data;
	int i;
	for(data = buff, i = 0; i < num; i++, data++) {
		data = strchr(data, ch);
		if(!data) {
			return NULL;
		}
	}
	return data;
}
/*
void find_target(char *out, int buffsz,char *data) {
	char tbuff[MAX_BUFF];
	char *tmp;
	memcpy(&tbuff,data,sizeof(tbuff));
	tmp=find_and_cut(tbuff,2,' ');
	sprintf_s(out,buffsz,"%s",tmp);
}
*/
void find_user_info(char *str, char *nick, char *user, char *address, int bufflen) {
	char *p;
	if(str[0]==':') {
		str++;
	}
	if(nick != NULL) {
		p=strchr(str,'!');
		if(p != NULL) {
			*p=0;
			sprintf_s(nick,bufflen,"%s",str);
			str=p+1;
			*p = '!';
		}
	}
	if(user != NULL) {
		char *x=NULL;
		p=strchr(str,'!');
		if(p != NULL) {
			x=p;
			p=strchr(x,'@');
			*p=0;
			sprintf_s(user,bufflen,"%s",x);
			*p='@';
		} else {
			x = strchr(str,'@');
			if(x != NULL) {
				*x=0;
				sprintf_s(user,bufflen,"%s",str);
				*x='@';
			}
		}
	}
	if(address != NULL) {
		p=strchr(str,'@');
		str=p+1;
		sprintf_s(address,bufflen,"%s",str);
	}
}
void gen_random(char *s, const int len) {
	int i;
//	srand((unsigned int)time(NULL));
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}
uint32_t resolv(char *host) {
    struct  hostent *hp;
    uint32_t    host_ip;

    host_ip = inet_addr(host);
    if(host_ip == INADDR_NONE) {
        hp = gethostbyname(host);
        if(!hp) {
			return (uint32_t)-1;
        }
        host_ip = *(uint32_t *)hp->h_addr;
    }
    return(host_ip);
}
int find_paramint(char *name, char *buff) {
	char intdst[24];
	if(!find_param(name,buff,(char *)&intdst,sizeof(intdst))) return 0;
	return atoi(intdst);
}
int find_paramint(int num, char *buff) {
	char intdst[24];
	if(!find_param(num,buff,(char *)&intdst,sizeof(intdst))) return 0;
	return atoi(intdst);
}
bool find_param(char *name, char *buff, char *dst, int dstlen) {

	char *loc;
	int distance;
	if(buff == NULL || name == NULL || dst == NULL || dstlen == 0) return false;
	if(*buff == '\\') {
		buff++;
	}
	memset(dst,0,dstlen);
	while(*buff) {
		loc = strchr(buff, '\\');
		if(loc != NULL) {
			distance = loc-buff;
			if(_strnicmp(buff,name,distance) == 0 && distance > 0) {
				buff+=distance+1;
				loc = strchr(buff,'\\');
				if(loc != NULL) {
					distance = loc-buff;		
					buff[distance] = 0;
				} 
				strncpy(dst,buff,strlen(buff)%dstlen);
				if(loc != NULL) {
					buff[distance] = '\\'; //restore it after!
				}
				return true;
			}
		}
		buff+=distance+1;
	}
	return false;
}
bool find_param(int num, char *buff, char *dst, int dstlen) {
	if(buff == NULL || dst == NULL || dstlen == 0) {
		return 0;
	}
	char *loc;
	int distance;
	int i=0;
	memset(dst,0,dstlen);
	if(buff[0] == '\\') {
		buff++;
	}
	while(*buff) {
		loc = strchr(buff, '\\');
		distance = loc-buff;
		if(i == num) {
			if(loc != NULL)
				*loc = 0;
			strncpy(dst,buff,strlen(buff)%dstlen);
			if(loc != NULL)
				*loc = '\\';
			return true;
		}
		i++;
		buff += distance+1;
		if(loc == NULL) break;
	}
	return false;
}
int match2(const char *mask, const char *name)
{
	const u_char *m = (u_char *)mask;
	const u_char *n = (u_char *)name;
	const u_char *ma = NULL;
	const u_char *na = (u_char *)name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 0;
			if (!ma)
				return 1;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 0;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m != 0);
		}
		
		if ((tolower(*m) != tolower(*n)) && !((*m == '_') && (*n == ' ')) && (*m != '?'))
		{
			if (!ma)
				return 1;
			m = ma;
			n = ++na;
		} else
		{
			m++;
			n++;
		}
	}
	return 1;
}
int match(const char *mask, const char *name) {
	if (mask[0] == '*' && mask[1] == '!') {
		mask += 2;
		while (*name != '!' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
		
	if (mask[0] == '*' && mask[1] == '@') {
		mask += 2;
		while (*name != '@' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
	return match2(mask,name);
}
void find_nth(char *p, int n, char *buff, int bufflen) {
	int x=n;
	char *c;
	char *tmpbuff=(char *)malloc(bufflen);
	if(tmpbuff == NULL) return;
	memcpy(tmpbuff,p,bufflen);
	int distance=0;
	memset(buff,0,bufflen);
	c = strtok(p," ");
	while(c != NULL) {
		x--;
		if(x<0) { 
			strncpy(buff,c,strlen(c)%bufflen);
			break;
		}
		c = strtok(NULL," ");
	}
	memcpy(p,tmpbuff,bufflen);
	free(tmpbuff);
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
int formatSend(int sd, bool putFinal, char enc, char *fmt, ...) {
	va_list vars;
	int len = 0;
	char buf[1024];
	memset(&buf,0,sizeof(buf));
	va_start (vars, fmt);
	len = vsnprintf(buf,sizeof(buf),fmt, vars);
	va_end(vars);
	if(putFinal) {
		strcat(buf,"\\final\\");
	}
	printf("sending: %s\n",buf);
	if(enc == 1) {
		len = strlen(buf);
		gamespyxor(buf,len);
	} else if(enc == 2) {
		len = strlen(buf);
//		if(!putFinal) len -= 7;//length of final + slashes
		gamespy3dxor(buf,len);
		if(putFinal == true) { //gamespy doesn't enc final part :X
//			strcat(buf,"\\final\\");
//			memcpy(buf+len,"\\final\\",7);
//			len+=7;
		}	
	} else {
		len = strlen(buf);
	}
	return send(sd,buf,len,MSG_NOSIGNAL);
}

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

    passlen = strlen((const char *)pass);
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
    for(*p = 0; len < 0; len++) *(p + len) = '_';

    if(size) *size = p - buff;
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
        len = strlen((const char *)data);
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

    len = p - buff;
    if(size) *size = len;
    if((len + 1) != xlen) {             // save memory
        buff = (uint8_t *)realloc(buff, len + 1);  // NULL included!
        if(!buff) return(NULL);
    }
    return(buff);
}
bool is_loweralpha(char ch) {
char allowed[]= "abcdefghijklmnopqrstuvwxyz";
	for(int i=0;i<sizeof(allowed);i++) {
		if(allowed[i] == ch) return true;
	}
	return false;
}
//moved from peerchat server.cpp for use with playerspy, etc
bool charValid(char ch) {
char allowed[]=
		"1234567890#"
		"_-`()$-=;/[]"
		"@+&%"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for(int i=0;i<sizeof(allowed);i++) {
		if(allowed[i]==ch) return true;
	}
	return false;
}
void makeValid(char *name) {
	for(int i=0;i<strlen(name);i++) {
		if(!charValid(name[i])) {
			name[i] = '_';
		}
	}
}
bool nameValid(char *name, bool peerchat) {
	char bad_starters[] = "#@+&%";
	for(int i=0;i<sizeof(bad_starters);i++) {
		if(name[0]==bad_starters[i]) {
			if(name[0] != '\\' || !peerchat)
				return false;
		}
	}
	for(int i=0;i<strlen(name);i++) {
		if(!charValid(name[i])) {
			if(name[i] != '\\' || !peerchat)
				return false;
		}
	}
	return true;
}
int countchar(char *str, char ch, int len) {
	int x=0,i=0;
	for(i=0;i<len;i++) {
		if(str[i] == ch) {
			x++;
		}
	}
	return x;
}
int countchar(char *str, char ch) {
	return countchar(str,ch,strlen(str));
}
int makeStringSafe(char *buff, int len) {
	char *obuff = (char *)malloc(len*2);
	memset(obuff,0,len*2);
	char *p = obuff;
	int i;
	for(i=0;i<len;i++) {
		*(obuff++) = buff[i];
//		if(buff[i] == '%') buff[i] = '_';
		if(buff[i] == '%') {
			*(obuff++) = '%';
		}
	}
	strncpy(buff,p,strlen(p)%len);
	free((void *)p);
	return i;
}
