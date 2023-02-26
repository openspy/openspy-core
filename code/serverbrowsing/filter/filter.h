#ifndef _FILTER_H
#include <string>
#include <map>

#include "CToken.h"

bool filterMatches(std::vector<CToken> token_list, std::map<std::string, std::string>& kvList);

#endif //_FILTER_H
