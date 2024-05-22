#include <serverbrowsing/filter/filter.h>


void test_string_like_expectpass() {
    std::map<std::string, std::string> kvList;
    kvList["hostname"] = "THE SERVER";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("hostname LIKE '%SERVER%'");
    if(!filterMatches(token_list, kvList)) {
        exit(1);
    }
}

void test_string_like_expectfail() {
    std::map<std::string, std::string> kvList;
    kvList["hostname"] = "SOME NAME";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("hostname LIKE '%SERVER%'");
    if(filterMatches(token_list, kvList)) {
        exit(1);
    }
}

int main() {
    

    test_string_like_expectpass();
    test_string_like_expectfail();
    return 0;
}