#include <OS/OpenSpy.h>
#include "config.h"
Config::Config(char *name) {
	fd = fopen(name,"rb");
	filepos = 0;
	parseError = false;
	parseArray = NULL;
	parseConfig();
}
Config::~Config() {
	std::list<configVar *>::iterator it = rootVariables.begin();
	configVar *var;
	while(it != rootVariables.end()) {
		var = *it;
		freeVar(var);
		it++;
	}

	fclose(fd);
}
void Config::freeVar(configVar *var) {
	if(var->name) {
		free((void *)var->name);
	}
	if(var->type == CTYPE_STR) {
		if(var->value.sptr) {
			free((void *)var->value.sptr);
		}
	}
	free((void *)var);
}
char Config::readChar() {
	if(!hasMoreBytes()) return 0;
	return filedata[filepos++];
}
char Config::peekChar() {
	if(!hasMoreBytes()) return 0;
	return filedata[filepos];
}
bool Config::hasMoreBytes() {
	return (filepos < filelen) ? true : false;
}
void Config::skipWhiteSpaces() {
	while(hasMoreBytes() && isspace(peekChar())) {
		readChar();
	}
}
void Config::parseConfig() {
	if(!fd) {
		return;
	}
	fseek(fd,0,SEEK_END);
	filelen = ftell(fd);
	fseek(fd,0,SEEK_SET);
	filedata = (char *)malloc(filelen);
	if(!filedata) {
		fclose(fd);
		return;
	}
	fread(filedata,filelen,1,fd);
	while(hasMoreBytes() && !parseError) {
		skipWhiteSpaces();
		if(!hasMoreBytes()) {
			return;
		}
		char firstChar = peekChar();
		switch(firstChar) {
			case '#': HandleDirective(); break;
			case '@': HandleMacroExpansion(); break;
			case '/': HandleComment(); break;
			case '}': {
				configVar *tvar = parseArray;
				if(tvar != NULL) {
					parseArray = (configVar *)tvar->owner;
				}
				skipWhiteSpaces();
				readChar();
				break;
			}
			default: { //var assignment
				std::string aIdentifier;
				ReadIdentifier(aIdentifier);
				skipWhiteSpaces();
				char opChar = readChar();

				if(opChar != '=' && opChar != '{') {
					parseError = true;
					return;
				}
				if(opChar == '{') {  //array
					configVar *tvar = (configVar *)malloc(sizeof(configVar));
					memset(tvar,0,sizeof(configVar));
					tvar->name = (char *)malloc(strlen(aIdentifier.c_str())+1);
					strcpy(tvar->name,aIdentifier.c_str());
					tvar->owner = (void *)parseArray;
					tvar->type = CTYPE_ARRAY;
					rootVariables.push_back(tvar);
					parseArray = tvar;
					if(parseArray == NULL) { parseError = true; break; }
				} else {
					handleVarAssignment(aIdentifier);
					break;
				}
				break;
			}
		}
	}
}
void Config::HandleDirective() {
}
void Config::HandleMacroExpansion() {
}
void Config::HandleComment() {
}
void Config::ReadIdentifier(std::string& theIdentifierOut)
{
	theIdentifierOut = "";

	// An identifier must start with a letter
	char aChar = peekChar();
	if (!isalpha(aChar)) {
		parseError = true;
		return;
	}

	// Keep reading until non-alphanumeric (underscore '_' is also ok)
	while(hasMoreBytes() && (isalnum(aChar) || aChar=='_'))
	{
		theIdentifierOut += readChar();
		aChar = peekChar();
	}
}
void Config::handleVarAssignment(std::string theIdentifier) {
	skipWhiteSpaces();
	char aChar = peekChar();
	configVar *var;
	var = (configVar *)malloc(sizeof(configVar));
	memset(var,0,sizeof(configVar));
	var->name = (char *) malloc(strlen(theIdentifier.c_str())+1);
	var->owner = (void *)parseArray;
	strcpy(var->name,theIdentifier.c_str());
	while(aChar != ';') {
		if(aChar == '\"') {
			var->type = CTYPE_STR;
			std::string theString;
			ReadString(theString);
			var->value.sptr = (char *)malloc(strlen(theString.c_str())+1);
			strcpy(var->value.sptr,theString.c_str());
			rootVariables.push_back(var);
		} else if(aChar == 'I') { //IP string to integer
			readChar();
			aChar = peekChar();
			if(aChar != '\"') {
				parseError = true;
				return;
			}
			var->type = CTYPE_INT;
			std::string ipStr;
			ReadString(ipStr);
			var->value.ival = OS::Address(ipStr.c_str()).GetIP();
			rootVariables.push_back(var);			
		} else if(isdigit(aChar)) {
			var->type = CTYPE_INT;
			var->value.ival = ReadNumber();
			rootVariables.push_back(var);
		}
		skipWhiteSpaces();
		aChar = readChar();
		if (aChar != ';') {
			parseError = true;
			return; 
		}
	}
}
void Config::ReadString(std::string& theStringOut) {
	char aChar = readChar();

	theStringOut = "";
	aChar = readChar();
	while (aChar != '\"' && hasMoreBytes())
	{
		theStringOut += aChar;
		aChar = readChar();
	}
}
int Config::ReadNumber()
{
	std::string aStringNumber;
	char aChar = peekChar();
	if(!isdigit(aChar))
		return 0;

	while (isdigit(peekChar()))
		aStringNumber += readChar();

	return atoi(aStringNumber.c_str());
}
configVar *Config::getRootConfigVar(const char *name) {
	std::list<configVar *>::iterator it = rootVariables.begin();
	configVar *var;
	while(it != rootVariables.end()) {
		var = *it;
		if(var->owner == NULL) {
			if(strcmp(name,var->name) == 0) {
				return var;
			}
		}
		it++;
	}
	return NULL;
}
std::list<configVar *> Config::getRootInfo() {
	return rootVariables;
}
configVar *Config::getRootArray(const char *name) {
	std::list<configVar *>::iterator it = rootVariables.begin();
	configVar *var;
	while(it != rootVariables.end()) {
		var = *it;
		if(var->owner == NULL) {
			if(var->type == CTYPE_ARRAY) {
				if(strcmp(name,var->name) == 0) {
					return var;
				}
			}
		}
		it++;
	}
	return NULL;
}
std::list<configVar *> Config::getArrayVariables(configVar *arrayptr) {
	std::list<configVar *>::iterator it = rootVariables.begin();
	std::list<configVar *> rlist;
	configVar *var;
	while(it != rootVariables.end()) {
		var = *it;
		if(var->owner == arrayptr) {
			rlist.push_back(var);
		}
		it++;
	}	
	return rlist;
}
char *Config::getArrayString(configVar *arrayptr, const char *name) {
	std::list<configVar *> varlist = getArrayVariables(arrayptr);
	std::list<configVar *>::iterator it = varlist.begin();
	configVar *var;
	while(it != varlist.end()) {
		var = *it;
		if(var->type == CTYPE_STR) {
			if(strcmp(name,var->name) == 0) {
				return var->value.sptr;
			}
		}
		it++;
	}
	return NULL;
}
int Config::getArrayInt(configVar *arrayptr, const char *name) {
	std::list<configVar *> varlist = getArrayVariables(arrayptr);
	std::list<configVar *>::iterator it = varlist.begin();
	configVar *var;
	while(it != varlist.end()) {
		var = *it;
		if(var->type == CTYPE_INT) {
			if(strcmp(name,var->name) == 0) {
				return var->value.ival;
			}
		}
		it++;
	}
	return 0;
}
configVar *Config::getArrayArray(configVar *arrayptr, const char *name) {
	std::list<configVar *> varlist = getArrayVariables(arrayptr);
	std::list<configVar *>::iterator it = varlist.begin();
	configVar *var;
	while (it != varlist.end()) {
		var = *it;
		if (var->type == CTYPE_ARRAY) {
			if (strcmp(name, var->name) == 0) {
				return var;
			}
		}
		it++;
	}
	return NULL;
}
char *Config::getRootString(const char *name) {
	configVar *var;
	if((var = getRootConfigVar(name)) == NULL) {
		return NULL;
	}
	if(var->type == CTYPE_STR) {
		return var->value.sptr;
	}
	return NULL;
}
int Config::getRootInteger(const char *name) {
	configVar *var;
	if((var = getRootConfigVar(name)) == NULL) {
		return 0;
	}
	if(var->type == CTYPE_INT) {
		return var->value.ival;
	}
	return 0;
}
