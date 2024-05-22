#include <serverbrowsing/filter/filter.h>


void test_emptyserver_hasplayerscheck() {
    std::map<std::string, std::string> kvList;
    kvList["numplayers"] = "0";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("numplayers>0");
    if(filterMatches(token_list, kvList)) {
        exit(1);
    }
}

void test_singleplayerserver_hasplayerscheck() {
    std::map<std::string, std::string> kvList;
    kvList["numplayers"] = "1";

    
    std::vector<CToken> token_list = CToken::filterToTokenList("numplayers>0");
    if(!filterMatches(token_list, kvList)) {
        exit(1);
    }
}

int main() {
    

    test_emptyserver_hasplayerscheck();
    test_singleplayerserver_hasplayerscheck();
    return 0;
}