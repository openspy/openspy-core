#include <serverbrowsing/filter/filter.h>


void test_subtract_expectpass() {
    std::map<std::string, std::string> kvList;
    
    kvList["maxplayers"] = "2";
    kvList["numplayers"] = "1";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("maxplayers-numplayers == 1");
    if(!filterMatches(token_list, kvList)) {
        exit(1);
    }
}

void test_subtract_expectfail() {
    std::map<std::string, std::string> kvList;
    
    kvList["maxplayers"] = "999";
    kvList["numplayers"] = "1";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("maxplayers-numplayers == 1");
    if(filterMatches(token_list, kvList)) {
        exit(1);
    }
}

int main() {
    test_subtract_expectpass();
    test_subtract_expectfail();
    return 0;
}