#include "enctype_shared.h"
typedef struct {
unsigned char  enc1key[261];
char *keyptr;
} enctype1_data;
extern const unsigned char enctype1_cryptdata[256 + 1];
void func1(unsigned char *, int, enctype1_data *);
void func2(unsigned char *, int, unsigned char *);
void func3(unsigned char *, int, unsigned char *);
void func6(unsigned char *, int, enctype1_data *);
int  func7(int, enctype1_data *);
void func4(unsigned char *, int, enctype1_data *);
int  func5(int, unsigned char *, int, int *, int *, enctype1_data *);
void func8(unsigned char *, int, const unsigned char *);
