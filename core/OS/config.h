#ifndef _OPENSPY_CONFIG_INC
#define _OPENSPY_CONFIG_INC
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <list>
#include <OS/OpenSpy.h>
enum ConfigType {CTYPE_INT, CTYPE_BOOL, CTYPE_STR, CTYPE_ARRAY};
typedef struct  _configVar {
	char *name;
	ConfigType type;
	void *owner; //pointer to configVar
	union {
		int ival;
		bool bval;
		char *sptr;
		void *arrayptr; //ptr to another configVar
	} value;
} configVar;
typedef struct _arrayInfo {
	char *name;
	configVar *variables;
} arrayInfo;
class Config {
public:
	Config(char *filename);
	~Config();
	char *getRootString(const char *name);
	int getRootInteger(const char *name);
	configVar *getRootConfigVar(const char *name);
	std::list<configVar *> getRootInfo();
	std::list<configVar *> getArrayVariables(configVar *arrayptr);
	char *getArrayString(configVar *arrayptr, const char *name);
	int getArrayInt(configVar *arrayptr, const char *name);
	configVar *getArrayArray(configVar *arrayptr, const char *name);
	configVar *getRootArray(const char *name);
private:
	void ReadIdentifier(std::string& theIdentifierOut);
	void ReadString(std::string& theIdentifierOut);
	int ReadNumber();
	void handleVarAssignment(std::string theIdentifier);
	void HandleDirective();
	void HandleMacroExpansion();
	void HandleComment();
	char readChar();
	char peekChar();
	bool hasMoreBytes();
	void skipWhiteSpaces();
	void freeVar(configVar *var);
	void parseConfig();
	bool parseError;
	configVar *rootInfo;
	FILE *fd;
	char *filedata;
	int filepos;
	int filelen;
	std::list<configVar *> rootVariables;
	configVar *parseArray; //what array are we currently parsing data for
};
#endif
