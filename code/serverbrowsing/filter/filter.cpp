#include "filter.h"
#include "CToken.h"

bool filterMatches(std::vector<CToken> token_list, std::map<std::string, std::string>& kvList) {
	if (token_list.empty()) {
		return true;
	}
	return evaluate(token_list, kvList);
}
