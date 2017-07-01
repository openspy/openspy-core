#include "OpenSpy.h"

#include <sstream>
#include <curl/curl.h>


#include <OS/socketlib/socketlib.h>

#include "Auth.h"
#include "Logger.h"

#ifndef _WIN32
	#include "OS/Logger/Unix/UnixLogger.h"
#endif

#include <OS/legacy/helpers.h>

namespace OS {
	Logger *g_logger = NULL;
	redisContext *redis_internal_connection = NULL;
	void Init(const char *appName) {

		curl_global_init(CURL_GLOBAL_SSL);

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		redis_internal_connection = redisConnectWithTimeout(OS_REDIS_SERV, OS_REDIS_PORT, t);
		#ifndef _WIN32
			g_logger = new UnixLogger(appName);
		#endif
	}
	void Shutdown() {
		if(AuthTask::HasAuthTask()) {
			delete AuthTask::getAuthTask();
		}

		redisFree(redis_internal_connection);
		curl_global_cleanup();
	}
	OS::GameData GetGameByRedisKey(const char *key, redisContext *redis_ctx = NULL) {
		GameData game;
		redisReply *reply;

		if(redis_ctx == NULL) {
			redis_ctx = OS::redis_internal_connection;
		}

		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", ERedisDB_Game));

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s gameid", key);
		if (reply->type == REDIS_REPLY_STRING) {
			game.gameid = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);


		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s secretkey", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy(game.secretkey, OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s description", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy(game.description, OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s gamename", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy(game.gamename, OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s disabled_services", key);
		if (reply->type == REDIS_REPLY_STRING)
			game.disabled_services = atoi(OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s queryport", key);
		if (reply->type == REDIS_REPLY_STRING)
			game.queryport = atoi(OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);


		//for thugpro
		game.popular_values.push_back("SKATOPIA");
		game.popular_values.push_back("NEW ORLEANS");
		game.popular_values.push_back("LOS ANGELES");
		game.popular_values.push_back("SCHOOL");
		game.popular_values.push_back("Graffiti");

		//thugpro test
		game.push_keys["country"] = KEYTYPE_STRING;
		game.push_keys["gamemode"] = KEYTYPE_STRING;
		game.push_keys["gametype"] = KEYTYPE_STRING;
		game.push_keys["gamever"] = KEYTYPE_STRING;
		game.push_keys["hostname"] = KEYTYPE_STRING;
		game.push_keys["mapname"] = KEYTYPE_STRING;
		game.push_keys["maxplayers"] = KEYTYPE_BYTE;
		game.push_keys["numplayers"] = KEYTYPE_BYTE;
		game.push_keys["password"] = KEYTYPE_BYTE;

		return game;

	}
	OS::GameData GetGameByName(const char *from_gamename, redisContext *redis_ctx) {
		redisReply *reply;
		OS::GameData ret;
		if(redis_ctx == NULL) {
			redis_ctx = OS::redis_internal_connection;
		}
		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", ERedisDB_Game));

		memset(&ret, 0, sizeof(ret));
		int cursor = 0;
		do {
			reply = (redisReply *)redisCommand(redis_ctx, "SCAN %d MATCH %s:*", cursor, from_gamename);
		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if(reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			for(int i=0;i<reply->element[1]->elements;i++) {
				ret = GetGameByRedisKey(reply->element[1]->element[i]->str, redis_ctx);
				break;
			}
			freeReplyObject(reply);
		} while(cursor != 0);
		return ret;
	}
	OS::GameData GetGameByID(int gameid, redisContext *redis_ctx) {
		redisReply *reply;
		OS::GameData ret;
		if(redis_ctx == NULL) {
			redis_ctx = OS::redis_internal_connection;
		}
		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", ERedisDB_Game));

		memset(&ret, 0, sizeof(ret));
		int cursor = 0;
		do {
			reply = (redisReply *)redisCommand(redis_ctx, "SCAN %d MATCH *:%d", cursor, gameid);
		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if(reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			for(int i=0;i<reply->element[1]->elements;i++) {
				ret = GetGameByRedisKey(reply->element[1]->element[i]->str, redis_ctx);
				break;
			}
			freeReplyObject(reply);
		} while(cursor != 0);
		return ret;
	}
	std::map<std::string, std::string> KeyStringToMap(std::string input) {
		std::map<std::string, std::string> ret;

		std::stringstream ss(input);

		std::string key, token;

		int i = 0;
		while (std::getline(ss, token, '\\')) {
			if (!token.length()) {
				i++;
				continue;
			}

			if(i % 2) {
				key = token;
			} else {
				ret[key] = token;
			}

			i++;
		}
		return ret;

	}
	std::vector<std::string> KeyStringToVector(std::string input) {
		std::vector<std::string> ret;

		std::stringstream ss(input);

		std::string token;

		while (std::getline(ss, token, '\\')) {
			if (!token.length())
				continue;
			ret.push_back(token);
		}
		return ret;

	}
	std::string MapToKVString(std::map<std::string, std::string> kv_data) {
		std::ostringstream s;
		std::map<std::string, std::string>::iterator it = kv_data.begin();
		while(it != kv_data.end()) {
			std::pair<std::string, std::string> p = *it;
			s << "\\" << p.first << "\\" << p.second;
			it++;
		}
		return s.str();
	}
	CThread *CreateThread(OS::ThreadEntry *entry, void *param,  bool auto_start) {
		#if _WIN32
			return new OS::CWin32Thread(entry, param, auto_start);
		#else
			return new OS::CPThread(entry, param, auto_start);
		#endif
	}
	CMutex *CreateMutex() {
		#if _WIN32
			return new OS::CWin32Mutex();
		#else
			return new OS::CPMutex();
		#endif
	}
	std::string strip_quotes(std::string s) {
		if(s[0] != '"' || s[s.length()-1] != '"')
			return s;
		return s.substr(1, s.size() - 2);
	}
	std::string strip_whitespace(std::string s) {
		std::string ret;
		std::string::iterator it = s.begin();
		while(it != s.end()) {
			char ch = *(it++);
			if(isspace(ch))
				continue;
			ret += ch;
		}
		return ret;
	}
	void Sleep(int time_ms) {
		#ifdef _WIN32
		Sleep(time_ms);
		#else
		usleep(time_ms * 1000);
		#endif
	}
	Address::Address(uint32_t ip, uint16_t port) {
		this->ip = ip;
		this->port = port;
	}
	Address::Address(struct sockaddr_in addr) {
		ip = Socket::htonl(addr.sin_addr.s_addr);
		port = Socket::htons(addr.sin_port);
	}
	Address::Address(const char *str) {
		char address[16];
		const char *seperator = strrchr(str, ':');
		unsigned int len = strlen(str);
		if(seperator) {
			port = atoi(seperator+1);
			len = seperator - str;
		}
		if(len < sizeof(address)) {
			strncpy(address, str, len);
			address[len] = 0;
		}
		ip = Socket::inet_addr((const char *)&address);
	}
	Address::Address() {
		ip = 0;
		port = 0;
	}
	const struct sockaddr_in Address::GetInAddr() {
		struct sockaddr_in ret;
		ret.sin_family = AF_INET;
		memset(&ret.sin_zero,0,sizeof(ret.sin_zero));
		ret.sin_addr.s_addr = Socket::htonl(ip);
		ret.sin_port = Socket::htons(port);
		return ret;

	}
	std::string Address::ToString(bool ip_only) {
		struct sockaddr_in addr;
		addr.sin_port = Socket::htons(port);
		addr.sin_addr.s_addr = Socket::htonl(ip);
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);
		std::ostringstream s;
		s << ipinput;
		if(!ip_only) {
 			s << ":" << port;
		}
		return s.str();
	}

	template<typename Out>
	void split(const std::string &s, char delim, Out result) {
	    std::stringstream ss;
	    ss.str(s);
	    std::string item;
	    while (std::getline(ss, item, delim)) {
	        *(result++) = item;
	    }
	}


	std::vector<std::string> split(const std::string &s, char delim) {
	    std::vector<std::string> elems;
	    split(s, delim, std::back_inserter(elems));
	    return elems;
	}
	void LogText(ELogLevel level, const char *fmt, ...) {
		va_list args;
		va_start (args, fmt);
		g_logger->LogText(level, fmt, args);
		va_end(args);
	}

	std::string FindBestMatch(std::vector<std::string> matches, std::string name) {
		std::vector<std::string>::iterator it = matches.begin();
		int best_score = 0, match_result;
		std::string best_result;
		while(it != matches.end()) {
			std::string s = *it;
			if(match2(s.c_str(),name.c_str(), match_result) == 0) {
				if(match_result > best_score) {
					best_result = s;
					best_score = match_result;
				}
			}
			it++;
		}
		return best_result;
	}
}
