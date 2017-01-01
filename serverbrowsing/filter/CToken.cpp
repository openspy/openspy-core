#include "CToken.h"
#include <string>
#include <stack>
#include <sstream>


/*
“appnamever=’PS3POKER1.28V’ and (password=0 or password=1)
 and hostname<>'splitnum' and gameregion=’_SCEA’ and
 gamelanguage=’EN’ and
 gamevariant=2 and gametype=1
 and limits=0 and avchatmode=0”
 
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
	char *ret_str;
	int i;
	for(i=idx;i<len;i++) {
		if(filter[i] == '\'' || filter[i] == '"' || filter[i]=='’') break;
		str += filter[i];
	}
	idx = i; 
	if(filter[i] == '\'' || filter[i] == '"' || filter[i]=='’') idx++; //skip the " or '
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
	char *ret_str;
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
bool checkMultiCharToken(const char *filter, int &idx, int len, CToken **token) {

	if(idx+1 > len) return false; //can't be multibyte since this is the end

	if(strncmp(&filter[idx],"==", 2) == 0) {
		*token = new CToken(EToken_Equals, NULL);
		idx ++;
	}else  if(strncmp(&filter[idx],"=", 1) == 0) {
		*token = new CToken(EToken_Equals, NULL);
	} else if(strncmp(&filter[idx],"!=", 2) == 0 || strncmp(&filter[idx],"<>", 2) == 0) {
		*token = new CToken(EToken_NotEquals, NULL);
		idx ++;
	} else if(strncmp(&filter[idx],">=", 2) == 0) {
		*token = new CToken(EToken_GreaterEquals, NULL);
		idx ++;
	} else if(strncmp(&filter[idx],"<=", 2) == 0) {
		*token = new CToken(EToken_LessEquals, NULL);
		idx ++;
	} else if(strncmp(&filter[idx],"<", 1) == 0) {
		*token = new CToken(EToken_Less, NULL);
	} else if(strncmp(&filter[idx],">", 1) == 0) {
		*token = new CToken(EToken_Greater, NULL);
	} else if(strncmp(&filter[idx],">", 1) == 0) {
		*token = new CToken(EToken_Greater, NULL);
	} else if(strncmp(&filter[idx],"||", 2) == 0 || strncmp(&filter[idx],"OR", 2) == 0) {
		*token = new CToken(EToken_Or, NULL);
		idx ++;
	} else if(strncmp(&filter[idx],"&&", 2) == 0) {
		*token = new CToken(EToken_And, NULL);
		idx ++;
	} //same as above, but theres a different length :(
	else if(strncasecmp(&filter[idx],"AND", 3) == 0) {
		*token = new CToken(EToken_And, NULL);
		idx += 2;
	} else if(strncmp(&filter[idx],"(", 1) == 0) {
		*token = new CToken(EToken_LeftBracket, NULL);
	} else if(strncmp(&filter[idx],")", 1) == 0) {
		*token = new CToken(EToken_RightBracket, NULL);
	}else {
		return false;
	}
	return true;
}

bool isOperator(ETokenType token_type) {
	return token_type == EToken_Equals || token_type == EToken_NotEquals || token_type == EToken_Greater ||
		token_type == EToken_GreaterEquals || token_type == EToken_Less || token_type == EToken_LessEquals || token_type == EToken_Or || token_type == EToken_And
		|| token_type == EToken_Multiply || token_type == EToken_Divide || token_type == EToken_Add || token_type == EToken_Subtract;
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
	} else if(token_type == EToken_Add || token_type == EToken_Subtract) {
		return 4;
	} else if(token_type == EToken_Multiply || token_type == EToken_Divide) {
		return 5;
	}
	return 0;
}
std::vector<CToken *> CToken::convertToRPN(std::vector<CToken *> token_list) {
	std::vector<CToken *> tokens;
	std::vector<CToken *>::iterator it = token_list.begin();
	std::stack<CToken *> operator_stack;
	while(it != token_list.end()) {
		CToken* cur_token = *it;
		ETokenType type = cur_token->getType();
		if(type == EToken_Variable || type == EToken_Integer || type == EToken_String) { //catch all operands
			tokens.push_back(cur_token);
		} else if(isOperator(type)) {
			while(!operator_stack.empty() && isOperator(operator_stack.top()->getType())) {
				if(IsLAssocOperator(type) && getPrecedence(operator_stack.top()->getType()) >= getPrecedence(type)) {
					tokens.push_back(operator_stack.top());
					operator_stack.pop();
				} else break;
			}
			operator_stack.push(cur_token);
		} else if(type == EToken_LeftBracket) {
			operator_stack.push(cur_token);
		} else if(type == EToken_RightBracket) {
			while(!operator_stack.empty() && operator_stack.top()->getType() != EToken_LeftBracket) {
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
std::vector<CToken *> CToken::filterToTokenList(const char *filter) {
	std::vector<CToken *> tokens;
	CToken *token;
	std::string variable_name = "";
	int filterlen = strlen(filter);
	for(int i=0;i<filterlen;i++) {
		 if(isspace(filter[i])) { //skip white space
			 continue;
		}else if(checkMultiCharToken(filter, i, filterlen, &token)) {
			tokens.push_back(token);
		} else if(filter[i] == '\'' || filter[i] == '"' || filter[i]=='’') {
			//gets freed in token deconstructor
			std::string str = readString(filter, ++i, filterlen);
			token = new CToken(str);
			tokens.push_back(token);
		} else if(filter[i] =='-') {
			token = new CToken(EToken_Subtract, NULL);
			tokens.push_back(token);
		} else if(filter[i] =='+') {
			token = new CToken(EToken_Add, NULL);
			tokens.push_back(token);
		}  else if(filter[i] =='/') {
			token = new CToken(EToken_Divide, NULL);
			tokens.push_back(token);
		} else if(filter[i] =='*') {
			token = new CToken(EToken_Multiply, NULL);
			tokens.push_back(token);
		}else if(isdigit(filter[i])) { //begins with a digit at least.. so lets read it in
			//TODO: float check
			int intval;
			float floatval;
			if(read_integer(filter, i, filterlen, intval, floatval)) {
				//token = new CToken(EToken_Float, (void *)floatval);
			} else {
				token = new CToken(EToken_Integer, (void *)intval);
			}
			tokens.push_back(token);
		} 
		else { //variable
			const char *str = readVariable(filter, i, filterlen);
			if(str == NULL) continue;
			token = new CToken(EToken_Variable,std::string(str));
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
			ss << ((long)token->getExtra());
		break;
		case EToken_String:
			ss << token->getString();
		break;
		default:
			ss << (const char *)token->getExtra();
		break;
	}
	return ss.str();
}

#define DEFINE_BOOLEAN_OPERATION(funcname, _operator) CToken * funcname (std::stack<CToken *> &stack) { \
	bool val = true; \
	if (stack.size() >= 2) { \
	CToken *t1, *t2; \
	t2 = stack.top(); \
	stack.pop(); \
	t1 = stack.top(); \
	stack.pop(); \
	long i1, i2; \
	if (t1->getType() == EToken_Integer && t2->getType() == EToken_Integer) { \
	i1 = (long)t1->getExtra(); \
	i2 = (long)t2->getExtra(); \
	val = (i1 _operator i2); \
	} else if(t1->getType() == EToken_String || t2->getType() == EToken_String) {\
		std::string s1 = tokenToString(t1); \
		std::string s2 = tokenToString(t2); \
		val = s1 _operator s2;\
	} \
	delete t1; \
	delete t2; \
	return new CToken(EToken_Integer, (void *)val); \
	} \
else { \
	return NULL; \
} \
}

#define DEFINE_BOOLEAN_OPERATION_NOSTR(funcname, _operator) CToken * funcname (std::stack<CToken *> &stack) { \
	bool val = true; \
	if (stack.size() >= 2) { \
		CToken *t1, *t2; \
		t2 = stack.top(); \
		stack.pop(); \
		t1 = stack.top(); \
		if(!t1 || !t2) return false; \
		stack.pop(); \
		long i1, i2; \
		if (t1->getType() == EToken_Integer && t2->getType() == EToken_Integer) { \
			i1 = (long)t1->getExtra(); \
			i2 = (long)t2->getExtra(); \
			val = (i1 _operator i2); \
		}\
		delete t1; \
		delete t2; \
		return new CToken(EToken_Integer, (void *)val); \
	} \
	else { \
		return NULL; \
	} \
}

DEFINE_BOOLEAN_OPERATION(equals, == )
DEFINE_BOOLEAN_OPERATION(nequals, != )
DEFINE_BOOLEAN_OPERATION(gequals, >= )
DEFINE_BOOLEAN_OPERATION(lequals, <= )
DEFINE_BOOLEAN_OPERATION_NOSTR(lessthan, <)
DEFINE_BOOLEAN_OPERATION_NOSTR(greaterthan, >)
DEFINE_BOOLEAN_OPERATION_NOSTR(evaland, &&)
DEFINE_BOOLEAN_OPERATION_NOSTR(evalor, || )

#define DEFINE_MATH_OPERATION(funcname, _operator) CToken *funcname(std::stack<CToken *> &stack) { \
	CToken *return_token = NULL; \
	int isum = 0; \
	float fsum = 0.0f; \
while (!stack.empty()) { \
	if(stack.top() == NULL) return NULL; \
	switch (stack.top()->getType()) { \
	case EToken_Integer: \
	isum _operator (long)stack.top()->getExtra(); \
	break; \
} \
	delete stack.top();\
	stack.pop(); \
} \
if (fsum != 0.0f) { \
} \
else if (isum != 0) { \
	return_token = new CToken(EToken_Integer, (void *)isum); \
} \
return return_token; \
}

DEFINE_MATH_OPERATION(add, +=)
DEFINE_MATH_OPERATION(subtract, -= )
DEFINE_MATH_OPERATION(multiply, *= )
DEFINE_MATH_OPERATION(divide, /= )

void fix_divide_zero(std::stack<CToken *> stack) {
	CToken *t1, *t2;
}

bool evaluate(std::vector<CToken *> tokens, std::map<std::string, std::string>& kvList) {
	std::vector<CToken *>::iterator it = tokens.begin();
	CToken *ret;
	std::stack<CToken *> operand_stack;
	while(it != tokens.end()) {
		if(isOperand((*it)->getType())) {
			if ((*it)->getType() == EToken_Variable) {
				TokenOperand tok = resolve_variable((const char *)(*it)->getString().c_str(),kvList);
				if(tok.token != EToken_String) {
					ret = new CToken(tok.token, (void *)tok.ptr);
				} else {
					ret = new CToken(tok.sval);
				}
				operand_stack.push(ret);
			} else
				operand_stack.push(*it);
		} else if(isOperator((*it)->getType())) {
			switch((*it)->getType()) {
				case EToken_Add:
					ret = add(operand_stack);
					operand_stack.push(ret);
				break;
				case EToken_Subtract:
					ret = subtract(operand_stack);
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
			}
			if(operand_stack.top() == NULL) return false;
			delete *it;
		}
		it++;
	}
	bool retval = false;
	if (!operand_stack.empty()) {
		ret = operand_stack.top();
		if(!ret) return false;
		retval = ret->getType() == EToken_Integer && ret->getExtra() != 0;
		delete ret;
	}
	return retval;
}
TokenOperand resolve_variable(CToken *token, std::map<std::string, std::string>& kvList) {
	TokenOperand ret;
	memset(&ret, 0, sizeof(ret));
	if(token->getType() == EToken_Variable)
		return resolve_variable((const char *)token->getExtra(), kvList);
	return ret;
}
TokenOperand resolve_variable(const char *name, std::map<std::string, std::string>& kvList) {
	TokenOperand ret;
	const char *var = kvList[name].c_str();
	if(var != NULL && atoi(var) != 0 || (var != NULL && var[0] =='0' && var[1] ==0 )) {
		ret.token = EToken_Integer;
		ret.ival = atoi(var);
	} else if(var != NULL) {
		ret.token = EToken_String;
		ret.sval = var;
	} else {
		ret.token = EToken_Integer;
		ret.ival = 0;
	}
	return ret;
}

 
