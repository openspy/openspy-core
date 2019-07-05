/******
gutil.h
GameSpy C Engine SDK
  
Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com

******

 Please see the GameSpy C Engine SDK documentation for more 
 information

******/
#include <OS/OpenSpy.h>
typedef unsigned char uchar;


void cengine_gs_encode ( uchar *ins, int size, uchar *result );
void cengine_gs_encrypt ( uchar *key, int key_len, uchar *buffer_ptr, int buffer_len );


		  
#define CRYPT_HEIGHT 16   
#define CRYPT_TABLE_SIZE       256 
#define NUM_KEYSETUP_PASSES 2 
#define NWORDS 16

typedef struct {
  uint32_t F[256];          
  uint32_t x_stack[CRYPT_HEIGHT];
  uint32_t y_stack[CRYPT_HEIGHT];
  uint32_t z_stack[CRYPT_HEIGHT];
  int index;      
  uint32_t x, y, z;
  uint32_t tree_num;
  uint32_t keydata[NWORDS];
 unsigned char *keyptr;

} crypt_key;

void
init_crypt_key(const unsigned char *key,
		   unsigned int bytes_in_key,
		   crypt_key *L);

void
crypt_encrypt(crypt_key *L, uint32_t *dest, int nodes);
void crypt_docrypt(crypt_key *L, unsigned char *data, int datalen);
