#include "filter.h"
#include "CToken.h"

//this must be changed so that the filter list isn't reallocated constantly for each server.. this is just temp
bool filterMatches(const char *filter, std::map<std::string, std::string>& kvList) {
	//std::vector<CToken *> filterToTokenList(const char *filter, std::vector<void *> &token_data);
	if(filter == NULL || strlen(filter) == 0) {
		return true;
	}
	std::vector<CToken> token_list = CToken::filterToTokenList((const char *)filter);

	bool ret = evaluate(token_list, kvList);

	//freeTokenList(token_list);
	return ret;	
}
