#ifndef OS_FILTER_TOKEN_H
#define OS_FILTER_TOKEN_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
enum ETokenType {
	EToken_None = -1,
	EToken_Variable, //variable representing int, string, etc
	EToken_Equals, //==, =
	EToken_NotEquals, //!=, <>
	EToken_Greater, //>
	EToken_GreaterEquals, //>=
	EToken_Less, //<
	EToken_LessEquals, //<=
	EToken_Or, // ||, OR,
	EToken_And, //&&, AND
	EToken_LeftBracket, //(
	EToken_RightBracket, //)
	EToken_Integer,
	EToken_String,
	EToken_Float,
	EToken_Add,
	EToken_Subtract,
	EToken_Multiply,
	EToken_Divide

};
typedef struct {
	union {
		float fval;
		long ival;
		void *ptr;
	};
	std::string sval;
	ETokenType token;
} TokenOperand;
class CToken {
public:
	CToken() : m_extra(NULL), m_type(EToken_None) {}
	CToken(ETokenType type, void *extra, bool no_free = false) :  m_extra (extra), m_type(type) {}
	CToken(std::string extra) : m_str(extra), m_type(EToken_String), m_extra(NULL) {}
	CToken(ETokenType type, std::string extra) :  m_str (extra), m_type(type), m_extra(NULL) {}
	~CToken();
	ETokenType getType() { return m_type; }
	void *getExtra() { return m_extra; }
	std::string getString() { return m_str; }
	static std::vector<CToken> filterToTokenList(const char *filter);
	static std::vector<CToken> convertToRPN(std::vector<CToken> token_list);
private:
	bool no_free;
	ETokenType m_type;
	void *m_extra; //ptr to int, null terminated string, etc if variable
	std::string m_str;
};
TokenOperand resolve_variable(CToken token, std::map<std::string, std::string>& kvList);
TokenOperand resolve_variable(const char *name, std::map<std::string, std::string>& kvList);
bool evaluate(std::vector<CToken> tokens, std::map<std::string, std::string>& kvList);
#endif
