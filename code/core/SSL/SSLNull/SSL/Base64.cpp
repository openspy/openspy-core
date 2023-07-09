#include <OS/OpenSpy.h>
#include "md5.h"

uint8_t *sslnull_base64_encode(const uint8_t *data, size_t *size);
uint8_t *sslnull_base64_decode(const uint8_t *data, size_t *size);

namespace OS {
	//////////////////////////
	/////// Base 64

	void Base64StrToBin(const char *str, uint8_t **out, size_t &len) {
		*out = sslnull_base64_decode((const uint8_t*)str, &len);
	}
	const char *BinToBase64Str(const uint8_t *in, size_t in_len) {
		return (const char *)sslnull_base64_encode(in, &in_len);
	}

	//////////////////////////////
	/////// MD5
	const char *MD5String(const char *string) {
		uint8_t md5_output[16]; //raw md5 data
		char output[33];

		md5_context ctx;
		md5_starts(&ctx);
		md5_update(&ctx, (uint8_t *)string, strlen(string));
		md5_finish(&ctx, (uint8_t *)&md5_output);

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