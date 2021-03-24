#include "StringCrypter.h"
#include <OS/OpenSpy.h>
#include <openssl/pem.h>
namespace OS {
    StringCrypter::StringCrypter(std::string PEM_privateKey_Path, std::string pemPassword) {
			FILE *fd = fopen(PEM_privateKey_Path.c_str(), "r+");
			mp_rsa_key = PEM_read_RSAPrivateKey(fd, NULL, NULL, (void *)pemPassword.c_str());
			fclose(fd);
    }

    StringCrypter::~StringCrypter() {
        RSA_free(mp_rsa_key);
    }
    std::string StringCrypter::encryptString(std::string input) {
			std::string ret;
			int mem_len = RSA_size(mp_rsa_key);
			unsigned char *buf = (unsigned char *)malloc(mem_len);
			RSA_public_encrypt(input.length() + 1, (const unsigned char *)input.c_str(), buf, mp_rsa_key, RSA_PKCS1_PADDING);
			const char *b64_str = OS::BinToBase64Str(buf, mem_len);
			ret = std::string(b64_str);
			free((void *)b64_str);
			free((void *)buf);
			return ret;
    }
    std::string StringCrypter::decryptString(std::string input) {
			std::string ret;
			uint8_t *b64_out;
			size_t out_len = 0;

			int mem_len = RSA_size(mp_rsa_key);
			unsigned char *buf = (unsigned char *)malloc(mem_len);

			OS::Base64StrToBin(input.c_str(), &b64_out, out_len);
			RSA_private_decrypt(out_len, b64_out, buf, mp_rsa_key, RSA_PKCS1_PADDING);
			ret = std::string((char *)buf);
			if (b64_out)
				free((void *)b64_out);

			if (buf) {
				free((void *)buf);
			}
			return ret;
    }
}