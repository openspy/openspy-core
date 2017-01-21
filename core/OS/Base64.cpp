#include "OpenSpy.h"
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/md5.h>
namespace OS {
	//////////////////////////
	/////// Base 64
	size_t calcDecodeLength(const char* b64input) { //Calculates the length of a decoded string
		size_t len = strlen(b64input),
			padding = 0;

		if (b64input[len-1] == '=' && b64input[len-2] == '=') //last two chars are =
			padding = 2;
		else if (b64input[len-1] == '=') //last char is =
			padding = 1;

		return (len*3)/4 - padding;
	}
	void Base64StrToBin(const char *str, uint8_t **out, int &len) {
		BIO *bio, *b64;

		int decodeLen = calcDecodeLength(str);
		*out = (uint8_t*)malloc(decodeLen + 1);
		memset(*out, 0,decodeLen + 1);

		bio = BIO_new_mem_buf((void *)str, -1);
		b64 = BIO_new(BIO_f_base64());
		bio = BIO_push(b64, bio);

		BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Do not use newlines to flush buffer
		len = BIO_read(bio, (char *)*out, strlen(str));
		//assert(*length == decodeLen); //length should equal decodeLen, else something went horribly wrong
		BIO_free_all(bio);

	}
	const char *BinToBase64Str(uint8_t *in, int in_len) {
		BIO *bio, *b64;
		BUF_MEM *bufferPtr;

		b64 = BIO_new(BIO_f_base64());
		bio = BIO_new(BIO_s_mem());
		bio = BIO_push(b64, bio);

		BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line

		BIO_write(bio, in, in_len);
		BIO_flush(bio);
		BIO_get_mem_ptr(bio, &bufferPtr);
		BIO_set_close(bio, BIO_NOCLOSE);
		BIO_free_all(bio);

		char *ret = (char *)malloc((*bufferPtr).length + 1);
		ret[(*bufferPtr).length] = 0;
		memcpy(ret, (*bufferPtr).data, (*bufferPtr).length);
		return  ret;
	}
	//////////////////////////////
	/////// MD5
	const char *MD5String(const char *string) {
		MD5_CTX ctx;
		uint8_t md5_output[16]; //raw md5 data
		char output[33];
		MD5_Init(&ctx);
 		MD5_Update(&ctx, string,strlen(string));
 		MD5_Final((uint8_t *)&md5_output, &ctx);

 		char *out = (char *)&output;
		const static char   hex[] = "0123456789abcdef";
	    for(int i = 0; i < 16; i++) {
	        *out++ = hex[md5_output[i] >> 4];
	        *out++ = hex[md5_output[i] & 15];
	    }
	    *out = 0;
	    return strdup(output);

	}
}