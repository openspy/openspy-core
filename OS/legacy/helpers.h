#ifndef _HELPER_INC
#define _HELPER_INC
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <WinSock.h>
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define snprintf sprintf_s
#define MSG_NOSIGNAL 0
#else
#include <arpa/inet.h>
#include <netdb.h>
#define stricmp strcasecmp
#define sprintf_s snprintf
#define strnicmp strncasecmp
#define vsprintf_s vsnprintf
#define _strnicmp strnicmp
#endif

char *find_last(char *buff, char delim, int len);
int find_end(char *buff, int len);
char *find_and_cut(char *buff, int num, char delim);
void strip(char *buff, char delim);
void find_and_replace(char *buff, char delim, int ch);
char *get_irc_command(char *buff, int *cmd);
int delimit(char *data);
char *find_first(char *buff, char delim, int len);
char *find_str(char *buff, int num, char ch);
//void find_target(char *out, int buffsz,char *data);
void find_user_info(char *str, char *nick, char *user, char *address, int bufflen); //bufflen is expected to be the size of all buffers
void gen_random(char *s, const int len);
uint32_t resolv(char *host);
int match(const char *mask, const char *name);
int match2(const char *mask, const char *name);
void find_nth(char *p, int n, char *buff, int bufflen);
bool find_param(char *name, char *buff, char *dst, int dstlen);
int find_paramint(char *name, char *buff);
bool find_param(int num, char *buff, char *dst, int dstlen);
int find_paramint(int num, char *buff);
void gamespyxor(char *data, int len);
void gamespy3dxor(char *data, int len);
int gspassenc(uint8_t *pass);
uint8_t *base64_encode(uint8_t *data, int *size);
uint8_t *base64_decode(uint8_t *data, int *size);
int formatSend(int sd, bool putFinal, char enc, char *fmt, ...);
bool is_loweralpha(char ch);
bool charValid(char ch);
void makeValid(char *name);
bool nameValid(char *name, bool peerchat);
int countchar(char *str, char ch);
int makeStringSafe(char *buff, int len);
#endif
