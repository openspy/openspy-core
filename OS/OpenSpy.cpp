#include "OpenSpy.h"

#include <sstream>

namespace OS {
	redisContext *redis_connection = NULL;
	void Init() {
		
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);
	}
	OS::GameData GetGameByRedisKey(const char *key) {
		GameData game;
		redisReply *reply;	
		freeReplyObject(redisCommand(OS::redis_connection, "SELECT %d", ERedisDB_Game));

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s gameid", key);
		if (reply->type == REDIS_REPLY_STRING)
			game.gameid = atoi(reply->str);
		freeReplyObject(reply);


		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s secretkey", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy_s(game.secretkey, sizeof(game.secretkey), reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s description", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy_s(game.description, sizeof(game.description), reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s gamename", key);
		if (reply->type == REDIS_REPLY_STRING)
			strcpy_s(game.gamename, sizeof(game.gamename), reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s disabled_services", key);
		if (reply->type == REDIS_REPLY_STRING)
			game.disabled_services = atoi(reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s queryport", key);
		if (reply->type == REDIS_REPLY_STRING)
			game.queryport = atoi(reply->str);
		freeReplyObject(reply);

		return game;
		
	}
	OS::GameData GetGameByName(const char *from_gamename) {
		redisReply *reply;
		freeReplyObject(redisCommand(OS::redis_connection, "SELECT %d", ERedisDB_Game));
		OS::GameData ret;
		memset(&ret, 0, sizeof(ret));
		reply = (redisReply *)redisCommand(OS::redis_connection, "KEYS %s:*",from_gamename);
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				ret = GetGameByRedisKey(reply->element[j]->str);
				break;
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	OS::GameData GetGameByID(int gameid) {
		redisReply *reply;
		freeReplyObject(redisCommand(OS::redis_connection, "SELECT %d", ERedisDB_Game));
		OS::GameData ret;
		memset(&ret, 0, sizeof(ret));
		reply = (redisReply *)redisCommand(OS::redis_connection, "KEYS *:%d:", gameid);
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				ret = GetGameByRedisKey(reply->element[j]->str);
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
}