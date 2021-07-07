#include <string.h>
#include "irc_common.h"
#define NICKLEN 32
typedef unsigned char u_char;
#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32
#define GAMESPY 64
#define BLOCK_START 128
#define	IsCntrl(c) (char_atribs[(u_char)(c)]&CNTRL)
#define IsAlpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define IsSpace(c) (char_atribs[(u_char)(c)]&SPACE)
#define IsLower(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) > 0x5f))
#define IsUpper(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) < 0x60))
#define IsDigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	IsXDigit(c) (isdigit(c) || 'a' <= (c) && (c) <= 'f' || \
                      'A' <= (c) && (c) <= 'F')
#define IsAlnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define IsPrint(c) (char_atribs[(u_char)(c)]&PRINT)
#define IsAscii(c) ((u_char)(c) >= 0 && (u_char)(c) <= 0x7f)
#define IsGraph(c) ((char_atribs[(u_char)(c)]&PRINT) && ((u_char)(c) != 0x32))
#define IsPunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))
#define IsGamespy(c) ((char_atribs[(u_char)(c)]&(GAMESPY)))
#define IsBlockStart(c) ((char_atribs[(u_char)(c)]&(BLOCK_START)))

#define	isvalid(c) (((c) >= 'A' && (c) < '~') || IsDigit(c) || (c) == '-' || IsGamespy(c))

unsigned char char_atribs[] =
{
    /* 0-7 */
    CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
    /* 8-12*/
    CNTRL, CNTRL | SPACE, CNTRL | SPACE, CNTRL | SPACE, CNTRL | SPACE,
    /* 13-15 */
    CNTRL | SPACE, CNTRL, CNTRL,
    /* 16-23 */
    CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
    /* 24-31 */
    CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
    /* space */
    PRINT | SPACE,
    /* !"#$%&'( */
    PRINT, PRINT /*| GAMESPY*/, PRINT | GAMESPY | BLOCK_START, PRINT | GAMESPY, PRINT | GAMESPY, PRINT | GAMESPY, PRINT /*| GAMESPY*/, PRINT | GAMESPY,
    /* )*+,-./ */
    PRINT | GAMESPY, PRINT | GAMESPY, PRINT | GAMESPY | BLOCK_START, PRINT | GAMESPY, PRINT | GAMESPY, PRINT | GAMESPY, PRINT | GAMESPY,
    /* 0123 */
    PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT,
    /* 4567 */
    PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT,
    /* 89:; */
    PRINT | DIGIT, PRINT | DIGIT, PRINT | GAMESPY | BLOCK_START, PRINT | GAMESPY,
    /* <=>? */
    PRINT, PRINT, PRINT, PRINT,
    /* @ */
    PRINT | GAMESPY | BLOCK_START,
    /* ABC */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* DEF */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* GHI */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* JKL */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* MNO */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* PQR */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* STU */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* VWX */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* YZ[ */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* \]^ */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* _` */
    PRINT, PRINT,
    /* abc */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* def */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* ghi */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* jkl */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* mno */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* pqr */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* stu */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* vwx */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* yz{ */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* \}~ */
    PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
    /* del */
    0,
    /* 80-8f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 90-9f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* a0-af */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* b0-bf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* c0-cf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* d0-df */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* e0-ef */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* f0-ff */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
 * * 'do_nick_name' ensures that the given parameter (nick) is * really
 * a proper string for a nickname (note, the 'nick' * may be modified
 * in the process...) *
 * 
 *      RETURNS the length of the final NICKNAME (0, if *
 * nickname is illegal) *
 * 
 *  Nickname characters are in range *  'A'..'}', '_', '-', '0'..'9' *
 * anything outside the above set will terminate nickname. *  In
 * addition, the first character cannot be '-' *  or a Digit. *
 * 
 *  Note: *     '~'-character should be allowed, but *  a change should
 * be global, some confusion would *    result if only few servers
 * allowed it...
 */

int is_nick_valid(char *nick) {
  char   *ch;

  if(strlen(nick) > NICKLEN) return 0;

  char start = nick[0];
  if(IsBlockStart(start)) return 0;
    
  for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
      if (!isvalid(*ch) || IsSpace(*ch))
	  break;
  
  //*ch = '\0';
  
  return (ch - nick);
}


int is_chan_valid(char *nick) {
  char   *ch;

  int len = strlen(nick);
  if(len > NICKLEN && len > 1) return 0;
  
  if (*(nick++) != '#')
      return 0;

  
  for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
      if (!IsAlnum(*ch))
	  break;
  
  //*ch = '\0';
  
  return (ch - nick);
}