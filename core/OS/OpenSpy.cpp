#include "OpenSpy.h"

#include <sstream>
#include <curl/curl.h>

namespace OS {
	redisContext *redis_internal_connection = NULL;
	void Init() {

		curl_global_init(CURL_GLOBAL_SSL);
		
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		redis_internal_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);
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
		OS::GameData ret;
		memset(&ret, 0, sizeof(ret));

		if(redis_ctx == NULL) {
			redis_ctx = OS::redis_internal_connection;
		}

		redisReply *reply;
		reply = (redisReply *)redisCommand(redis_ctx, "SELECT %d", ERedisDB_Game);
		if(!reply)
			return ret;
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(redis_ctx, "KEYS %s:*",from_gamename);
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				ret = GetGameByRedisKey(reply->element[j]->str, redis_ctx);
				break;
			}
		}
		freeReplyObject(reply);
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
		reply = (redisReply *)redisCommand(redis_ctx, "KEYS *:%d", gameid);
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				ret = GetGameByRedisKey(reply->element[j]->str, redis_ctx);
				break;
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	std::vector<std::string> KeyStringToMap(std::string input) {
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
}
