#include <OS/OpenSpy.h>
#include "CToken.h"
#include <string>
#include <stack>
#include <sstream>

/*
�appnamever=�PS3POKER1.28V� and (password=0 or password=1)
 and hostname<>'splitnum' and gameregion=�_SCEA� and
 gamelanguage=�EN� and
 gamevariant=2 and gametype=1
 and limits=0 and avchatmode=0�

 splitnum' and avchatmode=2 RET: 0
aa PS3POKER1.28 PS3POKER1.28
filter: appnamever='PS3POKER1.28' and (password=0 or password=1) and hostname<>'splitnum' and avchatmode=3 RET: 0
aa PS3POKER1.28 PS3POKER1.28
aa 0 splitnum
filter: appnamever='PS3POKER1.28' and (password=0 or password=1) and hostname<>'splitnum' RET: 1

*/
CToken::~CToken() {
	//now freed in the free token list function
	/*if(m_type == EToken_String) {
		if(m_extra != NULL) {
			free((void *)m_extra);
		}
	}
	*/
}
std::string readString(const char *filter, int &idx, int len) {
	std::string str = "";
	int i;
	for(i=idx;i<len;i++) {
		if(filter[i] == '\'' || filter[i] == '"') break;
		str += filter[i];
	}
	idx = i;
	if(filter[i] == '\'' || filter[i] == '"') idx++; //skip the " or '
	return str;
}
const char *readVariable(const char *filter, int &idx, int len) {
	char *ret_str;
	ret_str = (char *)malloc(len);
	memset(ret_str,0,len);
	int i;
	int c = 0;
	for(i=idx;i<len;i++) {
		if(!isalnum(filter[i]) && filter[i] != '.' && filter[i] != '_') {
			break;
		}
		ret_str[c++] = filter[i];
	}
	idx = i;
	if(strlen(ret_str) == 0) {
		free((void *)ret_str);
		return NULL;
	}
	return ret_str;
}
bool read_integer(const char *filter, int &idx, int len, int &int_val, float &float_val) {
	std::string str = "";
	int i;
	bool is_float = false;
	for(i=idx;i<len;i++) {
		if(filter[i] == 'f' || filter[i] =='.') { //float
			is_float = true;
			continue;
		}
		if(!isdigit(filter[i])) break;
		str += filter[i];
	}
	idx = i-1; //offset for loop increment
	if(is_float) {
		float_val = atof(str.c_str());
	} else {
		int_val = atoi(str.c_str());
	}
	return is_float;
}
bool checkMultiCharToken(const char *filter, int &idx, int len, CToken *token) {

	if (idx + 1 > len) {
		return false; //can't be multibyte since this is the end
	}

	if(strncmp(&filter[idx],"==", 2) == 0) {
		*token = CToken(EToken_Equals);
		idx ++;
	} else if (strncasecmp(&filter[idx], "is", 2) == 0) {
		*token = CToken(EToken_Equals);
		idx++;
	} else if (strncmp(&filter[idx], "=", 1) == 0) {
		*token = CToken(EToken_Equals);	}
	else if (strncmp(&filter[idx], "!=", 2) == 0 || strncmp(&filter[idx], "<>", 2) == 0) {
		*token = CToken(EToken_NotEquals);
		idx++;
	}
	else if(strncmp(&filter[idx],">=", 2) == 0) {
		*token = CToken(EToken_GreaterEquals);
		idx ++;
	} else if(strncmp(&filter[idx],"<=", 2) == 0) {
		*token = CToken(EToken_LessEquals);
		idx ++;
	} else if(strncmp(&filter[idx],"<", 1) == 0) {
		*token = CToken(EToken_Less);
	} else if(strncmp(&filter[idx],">", 1) == 0) {
		*token = CToken(EToken_Greater);
	} else if(strncmp(&filter[idx],"||", 2) == 0 || strncasecmp(&filter[idx],"OR", 2) == 0) {
		*token = CToken(EToken_Or);
		idx ++;
	} else if(strncmp(&filter[idx],"&&", 2) == 0) {
		*token = CToken(EToken_And);
		idx ++;
	} //same as above, but theres a different length :(
	else if(strncasecmp(&filter[idx],"AND", 3) == 0) {
		*token = CToken(EToken_And);
		idx += 2;
	} else if(strncmp(&filter[idx],"(", 1) == 0) {
		*token = CToken(EToken_LeftBracket);
	} else if(strncmp(&filter[idx],")", 1) == 0) {
		*token = CToken(EToken_RightBracket);
	} else if (strncasecmp(&filter[idx], "null", 4) == 0) {
		*token = CToken(0);
		idx += 3;
	}
	else if (strncasecmp(&filter[idx], "LIKE", 4) == 0) {
		*token = CToken(EToken_StrLike);
		idx += 3;
	}
	else if (strncasecmp(&filter[idx], "NOT", 3) == 0) {
		*token = CToken(EToken_Not);
		idx += 2;
	}
	else if (filter[idx] == '!') {
		*token = CToken(EToken_Not);
	}
	else {
		return false;
	}
	return true;
}

bool isOperator(ETokenType token_type) {
	return token_type == EToken_Equals || token_type == EToken_NotEquals || token_type == EToken_Greater ||
		token_type == EToken_GreaterEquals || token_type == EToken_Less || token_type == EToken_LessEquals || token_type == EToken_Or || token_type == EToken_And
		|| token_type == EToken_Multiply || token_type == EToken_Divide || token_type == EToken_Add || token_type == EToken_Subtract || token_type == EToken_StrLike || token_type == EToken_Not;
}

bool isOperand(ETokenType token_type) {
	return token_type == EToken_Variable || token_type == EToken_Integer || token_type == EToken_String || token_type == EToken_Float;
}

//atm all we have is lassoc operators so..
bool IsLAssocOperator(ETokenType token_type) {
	return isOperator(token_type);
}
int getPrecedence(ETokenType token_type) {
	if(token_type == EToken_Or) {
		return 1;
	} else if(token_type == EToken_And) {
		return 2;
	} else if(token_type == EToken_Equals || token_type == EToken_NotEquals) {
		return 3;
	}
	else if (token_type == EToken_Not) {
		return 4;
	} 
	else if (token_type == EToken_Add || token_type == EToken_Subtract) {
		return 4;
	}
	else if (token_type == EToken_Multiply || token_type == EToken_Divide) {
		return 5;
	}
	else if (token_type == EToken_Greater ||
		token_type == EToken_GreaterEquals || token_type == EToken_Less || token_type == EToken_LessEquals) {
		return 3;
	}
	else if (token_type == EToken_StrLike) {
		return 3;
	}
	return 0;
}
std::vector<CToken> CToken::convertToRPN(std::vector<CToken> token_list) {
	std::vector<CToken> tokens;
	std::vector<CToken>::iterator it = token_list.begin();
	std::stack<CToken> operator_stack;
	while(it != token_list.end()) {
		CToken cur_token = *it;
		ETokenType type = cur_token.getType();
		if(type == EToken_Variable || type == EToken_Integer || type == EToken_String) { //catch all operands
			tokens.push_back(cur_token);
		} else if(isOperator(type)) {
			while(!operator_stack.empty() && isOperator(operator_stack.top().getType())) {
				if(IsLAssocOperator(type) && getPrecedence(operator_stack.top().getType()) >= getPrecedence(type)) {
					tokens.push_back(operator_stack.top());
					operator_stack.pop();
				} else break;
			}
			operator_stack.push(cur_token);
		} else if(type == EToken_LeftBracket) {
			operator_stack.push(cur_token);
		} else if(type == EToken_RightBracket) {
			while(!operator_stack.empty() && operator_stack.top().getType() != EToken_LeftBracket) {
				tokens.push_back(operator_stack.top());
				operator_stack.pop();
			}

			if(!operator_stack.empty()) { //if this doesn't pass then there is a mismatch
				operator_stack.pop();
			}
		}
		it++;

	}

	while(!operator_stack.empty()) {
		tokens.push_back(operator_stack.top());
		operator_stack.pop();
	}
	return tokens;
}
std::vector<CToken> CToken::filterToTokenList(const char *filter) {
	std::vector<CToken> tokens;
	CToken token;
	std::string variable_name = "";
	int filterlen = strlen(filter);
	for(int i=0;i<filterlen;i++) {
		 if(isspace(filter[i])) { //skip white space
			 continue;
		}else if(checkMultiCharToken(filter, i, filterlen, &token)) {
			tokens.push_back(token);
		} else if(filter[i] == '\'' || filter[i] == '"') {
			//gets freed in token deconstructor
			std::string str = readString(filter, ++i, filterlen);
			token = CToken(str);
			tokens.push_back(token);
		} else if(filter[i] =='-') {
			token = CToken(EToken_Subtract);
			tokens.push_back(token);
		} else if(filter[i] =='+') {
			token = CToken(EToken_Add);
			tokens.push_back(token);
		}  else if(filter[i] =='/') {
			token = CToken(EToken_Divide);
			tokens.push_back(token);
		} else if(filter[i] =='*') {
			token = CToken(EToken_Multiply);
			tokens.push_back(token);
		}else if(isdigit(filter[i])) { //begins with a digit at least.. so lets read it in
			//TODO: float check
			int intval;
			float floatval;
			if(read_integer(filter, i, filterlen, intval, floatval)) {
				//token = CToken(EToken_Float, (void *)floatval);
			} else {
				token = CToken(intval);
			}
			tokens.push_back(token);
		}
		else { //variable
			const char *str = readVariable(filter, i, filterlen);
			if(str == NULL) continue;
			token = CToken(EToken_Variable,std::string(str));
			tokens.push_back(token);
			free((void *)str);

			i--; //offset for the next character
		}

	}
	return convertToRPN(tokens);
}
std::string tokenToString(CToken *token) {
	std::ostringstream ss;

	switch(token->getType()) {
		case EToken_Integer:
			ss << (token->getInt());
		break;
		case EToken_String:
			ss << token->getString();
		break;
		default:
			ss << token->getString();
		break;
	}
	return ss.str();
}

int string_compare_equals(std::string s1, std::string s2) {
	return s1.compare(s2);
}
int string_compare_like(std::string s1, std::string s2) {
	int match_count;
	return OS::match2(s2.c_str(), s1.c_str(), match_count, '%') == 0;
}

int string_compare_greater_than(std::string s1, std::string s2) {
	return s1.length() > s2.length();
}
int string_compare_less_than(std::string s1, std::string s2) {
	return s1.length() < s2.length();
}
int string_compare_greater_than_equals(std::string s1, std::string s2) {
	return s1.length() >= s2.length();
}
int string_compare_less_than_equals(std::string s1, std::string s2) {
	return s1.length() <= s2.length();
}
#define DEFINE_BOOLEAN_OPERATION(funcname, _operator, string_compare_func, string_compare_end) CToken funcname (std::stack<CToken> &stack) { \
	bool val = true; \
	if (stack.size() >= 2) { \
		CToken t1, t2; \
		t2 = stack.top(); \
		stack.pop(); \
		t1 = stack.top(); \
		stack.pop(); \
		if (t1.getType() == EToken_Integer && t2.getType() == EToken_Integer) { \
			int i1, i2; \
			i1 = t1.getInt(); \
			i2 = t2.getInt(); \
			val = i1 _operator i2; \
		} else if(t1.getType() == EToken_String || t2.getType() == EToken_String) {\
			std::string s2 = tokenToString(&t1); \
			std::string s1 = tokenToString(&t2); \
			val = string_compare_func (s1, s2) string_compare_end;\
		} \
	} \
	return CToken(val); \
}

#define DEFINE_BOOLEAN_OPERATION_NOSTR(funcname, _operator) CToken funcname (std::stack<CToken> &stack) { \
	bool val = true; \
	if (stack.size() >= 2) { \
		CToken t1, t2; \
		t2 = stack.top(); \
		stack.pop(); \
		t1 = stack.top(); \
		stack.pop(); \
		int i1, i2; \
		if (t1.getType() == EToken_Integer && t2.getType() == EToken_Integer) { \
			i1 = t1.getInt(); \
			i2 = t2.getInt(); \
			val = (i1 _operator i2); \
		}\
	} \
	return CToken(val); \
}

DEFINE_BOOLEAN_OPERATION(equals, == , string_compare_equals, == 0)
DEFINE_BOOLEAN_OPERATION(string_like, == , string_compare_like, == 1)
DEFINE_BOOLEAN_OPERATION(nequals, != , string_compare_equals, != 0)
DEFINE_BOOLEAN_OPERATION(gequals, >= , string_compare_greater_than_equals, ;)
DEFINE_BOOLEAN_OPERATION(lequals, <= , string_compare_less_than_equals, ;)
DEFINE_BOOLEAN_OPERATION_NOSTR(lessthan, <)
DEFINE_BOOLEAN_OPERATION_NOSTR(greaterthan, >)
DEFINE_BOOLEAN_OPERATION_NOSTR(evaland, &&)
DEFINE_BOOLEAN_OPERATION_NOSTR(evalor, || )

CToken divide(std::stack<CToken> &stack) {
	int isum = 0;
	float fsum = 0.0f;
	ETokenType type;
	CToken ret;
	if (stack.size() > 2) {
		CToken lh, rh;
		lh = stack.top(); stack.pop();
		rh = stack.top(); stack.pop();
		type = lh.getType();
		if (rh.getInt() == 0) return ret;
		switch (type) {
		case EToken_Integer:
			isum = lh.getInt() / rh.getInt();
			ret = CToken(isum);
			break;
		case EToken_Float:
			fsum = lh.getFloat() / rh.getFloat();
			ret = CToken(fsum);
			break;
		default:
		break;
		}

	}
	return ret;
}
CToken multiply(std::stack<CToken> &stack) {
	int isum = 0;
	float fsum = 0.0f;
	ETokenType type;
	CToken ret;
	if (stack.size() > 2) {
		CToken lh, rh;
		lh = stack.top(); stack.pop();
		rh = stack.top(); stack.pop();
		type = lh.getType();
		switch (type) {
		case EToken_Integer:
			isum = lh.getInt() / rh.getInt();
			ret = CToken(isum);
			break;
		case EToken_Float:
			fsum = lh.getFloat() * rh.getFloat();
			ret = CToken(fsum);
			break;
		default:
		break;
		}

	}
	return ret;
}


CToken addition(std::stack<CToken> &stack) {
	int isum = 0;
	float fsum = 0.0f;
	ETokenType type;
	CToken ret;
	if (stack.size() > 2) {
		CToken lh, rh;
		lh = stack.top(); stack.pop();
		rh = stack.top(); stack.pop();
		type = lh.getType();
		switch (type) {
		case EToken_Integer:
			isum = lh.getInt() + rh.getInt();
			ret = CToken(isum);
			break;
		case EToken_Float:
			fsum = lh.getFloat() + rh.getFloat();
			ret = CToken(fsum);
			break;
		default:
		break;
		}

	}
	return ret;
}

CToken subtraction(std::stack<CToken> &stack) {
	int isum = 0;
	float fsum = 0.0f;
	ETokenType type;
	CToken ret;
	if (stack.size() > 2) {
		CToken lh, rh;
		lh = stack.top(); stack.pop();
		rh = stack.top(); stack.pop();
		type = lh.getType();
		switch (type) {
		case EToken_Integer:
			isum = lh.getInt() - rh.getInt();
			ret = CToken(isum);
			break;
		case EToken_Float:
			fsum = lh.getFloat() - rh.getFloat();
			ret = CToken(fsum);
			break;
		default:
		break;
		}

	}
	return ret;
}

CToken logical_not(std::stack<CToken> &stack) {
	CToken ret(0);
	if (stack.size() >= 1) {
		CToken token;
		token = stack.top(); stack.pop();
		switch (token.getType()) {
		case EToken_Integer:
			ret = CToken(!token.getInt());
			break;
		case EToken_Float:
			ret = CToken(!token.getFloat());
			break;
		default:
		break;
		}
	}
	return ret;
}

/*#define DEFINE_MATH_OPERATION_ACCUM(funcname, _operator) CToken funcname(std::stack<CToken> &stack) { \
	CToken return_token; \
	int isum = 0; \
	float fsum = 0.0f; \
while (!stack.empty()) { \
	switch (stack.top().getType()) { \
	case EToken_Integer: \
	isum _operator (long)stack.top().getInt(); \
	break; \
} \
	stack.pop(); \
} \
if (fsum != 0.0f) { \
} \
else if (isum != 0) { \
	return_token = CToken(isum); \
} \
return return_token; \
}

DEFINE_MATH_OPERATION_ACCUM(add, +=)
DEFINE_MATH_OPERATION_ACCUM(subtract, -= )
DEFINE_MATH_OPERATION(multiply, *= )
DEFINE_MATH_OPERATION(divide, /= )*/


bool evaluate(std::vector<CToken> tokens, std::map<std::string, std::string>& kvList) {
	std::vector<CToken>::iterator it = tokens.begin();
	CToken ret;
	std::stack<CToken> operand_stack;
	while(it != tokens.end()) {
		if(isOperand((*it).getType())) {
			if ((*it).getType() == EToken_Variable) {
				TokenOperand tok = resolve_variable((const char *)(*it).getString().c_str(),kvList);
				switch (tok.token) {
					case EToken_String:
						ret = CToken(tok.sval);
						break;
					case EToken_Integer:
						ret = CToken(tok.ival);
						break;
					case EToken_Float:
						ret = CToken(tok.fval);
						break;
					default:
					break;
				}
				operand_stack.push(ret);
			} else
				operand_stack.push(*it);
		} else if(isOperator((*it).getType())) {
			switch((*it).getType()) {
				case EToken_Add:
					ret = addition(operand_stack);
					operand_stack.push(ret);
				break;
				case EToken_Subtract:
					ret = subtraction(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Multiply:
					ret = multiply(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Divide:
					ret = divide(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Equals:
					ret = equals(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_StrLike:
					ret = string_like(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_NotEquals:
					ret = nequals(operand_stack);
					operand_stack.push(ret);
				break;
				case EToken_LessEquals:
					ret = lequals(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_GreaterEquals:
					ret = gequals(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Less:
					ret = lessthan(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Greater:
					ret = greaterthan(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_And:
					ret = evaland(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Or:
					ret = evalor(operand_stack);
					operand_stack.push(ret);
					break;
				case EToken_Not:
					ret = logical_not(operand_stack);
					operand_stack.push(ret);
					break;
				default:
				break;
			}
		}
		it++;
	}
	bool retval = false;
	if (!operand_stack.empty()) {
		ret = operand_stack.top();
		retval = ret.getType() == EToken_Integer && ret.getInt() != 0;
	}
	return retval;
}
TokenOperand resolve_variable(CToken *token, std::map<std::string, std::string>& kvList) {
	TokenOperand ret;
	memset((void *)&ret, 0, sizeof(ret));
	if(token->getType() == EToken_Variable)
		return resolve_variable((const char *)token->getString().c_str(), kvList);
	return ret;
}

bool is_numeric(std::string numericString) {
	std::string::iterator it = numericString.begin();
	while (it != numericString.end()) {
		char ch = *it;
		if (!isdigit(ch)) {
			return false;
		}
		it++;
	}
	return true;

}
TokenOperand resolve_variable(const char *name, std::map<std::string, std::string>& kvList) {
	TokenOperand ret;
	const char *var = kvList[name].c_str();
	if(is_numeric(var)) {
		ret.token = EToken_Integer;
		ret.ival = atoi(var);
	} else if(var != NULL && strlen(var) > 0) {
		ret.token = EToken_String;
		ret.sval = var;
	} else {
		ret.token = EToken_Integer;
		ret.ival = 0;
	}
	return ret;
}
