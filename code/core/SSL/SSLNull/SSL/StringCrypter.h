#ifndef _STRINGCRYPTER_H
#define _STRINGCRYPTER_H
#include <string>
namespace OS {
    class StringCrypter {
        public:
            StringCrypter(std::string PEM_privateKey_Path, std::string pemPassword = "");
            ~StringCrypter();
            std::string encryptString(std::string input);
            std::string decryptString(std::string input);
        private:
    };
};
#endif //_STRINGCRYPTER_H