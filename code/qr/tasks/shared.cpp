#include <tasks/tasks.h>
#include <sstream>
namespace MM {
	bool isTeamString(const char *string) {
		size_t len = strlen(string);
		if(len < 2)
			return false;
		if(string[len-2] == '_' && string[len-1] == 't') {
			return true;
		}
		return false;
	}
	bool isPlayerString(std::string key, std::string &variable_name, int &player_id) {
		size_t last_underscore = key.find_last_of('_');
		if(last_underscore == std::string::npos || last_underscore-1 >= key.length())
			return false;
		std::string numeric_portion = key.substr(last_underscore + 1);

		std::string::const_iterator it = numeric_portion.begin();
		while (it != numeric_portion.end() && std::isdigit(*it)) ++it;
		if(it != numeric_portion.end()) return false;

		player_id = atoi(numeric_portion.c_str());
		variable_name = key.substr(0, last_underscore);
		return true;
	}
	int GetServerID(TaskThreadData *thread_data) {
		//Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		redisReply *resp = (redisReply *)redisCommand(thread_data->mp_redis_connection, "INCR %s", mp_pk_name);
		if(resp) {
			if(resp->type == REDIS_REPLY_STRING) {
				ret = atoi(resp->str);
			} else if(resp->type == REDIS_REPLY_INTEGER) {
				ret = resp->integer;
			}
			freeReplyObject(resp);
		}
		return ret;
	}

	int TryFindServerID(TaskThreadData *thread_data, OS::Address address) {
		std::string ip = address.ToString(true);
		std::stringstream map;
		map << "IPMAP_" << ip << "-" << address.GetPort();
		std::string map_str = map.str();
		redisReply *resp = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS %s", map_str.c_str());
		
		if(!resp) {
			return -1;
		}
		
		int ret = -1;
		if(resp) {
			if (resp->type == REDIS_REPLY_INTEGER) {
				ret = resp->integer;
			}
			else if (resp->type == REDIS_REPLY_STRING) {
				ret = atoi(resp->str);
			}
			freeReplyObject(resp);
		}
		
		if (ret == 1) {
			ret = -1;
			resp = (redisReply *)redisCommand(thread_data->mp_redis_connection, "GET %s", map_str.c_str());
			
			std::string server_key;
			if (resp) {
				if(resp->type == REDIS_REPLY_STRING) {
					server_key = resp->str;
				}
				freeReplyObject(resp);
			}

			resp = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s id", server_key.c_str());
			if(resp) {
				if(resp->type == REDIS_REPLY_INTEGER) {
					ret = resp->integer;
				} else if(resp->type == REDIS_REPLY_STRING) {
					ret = atoi(resp->str);
				}
				freeReplyObject(resp);
			}
			return ret;
		}
		else {
			return -1;
		}
	}
	OS::Address GetQueryAddressForServer(TaskThreadData *thread_data, std::string server_key) {
		OS::Address result;

		std::string ip;
		uint16_t port = 0;

		redisReply *reply;
		
		redisAppendCommand(thread_data->mp_redis_connection, "HGET %s wan_ip", server_key.c_str());
		redisAppendCommand(thread_data->mp_redis_connection, "HGET %s wan_port", server_key.c_str());

		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
		if(reply) {
			if(reply->type == REDIS_REPLY_STRING) {
				ip = reply->str;
			}
			freeReplyObject(reply);
		}
		
		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
		if(reply) {
			if(reply->type == REDIS_REPLY_STRING) {
				port = atoi(reply->str);
			} else if(reply->type == REDIS_REPLY_INTEGER) {
				port = reply->integer;
			}
			freeReplyObject(reply);
		}

		std::stringstream ss;
		ss << ip << ":" << port;

		result = OS::Address(ss.str());
		return result;
	}
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key, bool ignoreChallengeExists) {
		std::string ip;
		int ret = -1;
		bool challenge_exists = false;
		
		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s deleted", server_key.c_str());
		if(reply) {
			if(reply->type == REDIS_REPLY_INTEGER) {
				ret = reply->integer;
			} else if (reply->type == REDIS_REPLY_STRING) {
				ret = atoi(reply->str);
			}
			freeReplyObject(reply);
		}
		
		if(!ignoreChallengeExists) {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HEXISTS %s challenge", server_key.c_str());
			if(reply) {
				if(reply->type == REDIS_REPLY_INTEGER && reply->integer) {
					challenge_exists = true;
				} else if(reply->type == REDIS_REPLY_STRING && atoi(reply->str)) {
					challenge_exists = true;
				}
				freeReplyObject(reply);
			}
		}
		return ret == 1 && !challenge_exists;
	}
	int GetNumHeartbeats(TaskThreadData *thread_data, std::string server_key) {
		int ret = 0;
		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s num_updates", server_key.c_str());
		if(reply) {
			if(reply->type == REDIS_REPLY_INTEGER) {
				ret = reply->integer;
			} else if(reply->type == REDIS_REPLY_STRING) {
				ret = atoi(reply->str);
			}
			freeReplyObject(reply);
		}
		return ret;
	}

	int IncrNumHeartbeats(TaskThreadData *thread_data, std::string server_key) {
		int result = 0;
		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HINCRBY %s num_updates 1", server_key.c_str());
		if(reply) {
			result = reply->integer;
			freeReplyObject(reply);
		}
		return result;
	}

	void selectQRRedisDB(TaskThreadData *thread_data) {
		void *reply;
		reply = redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
		if(reply) {
			freeReplyObject(reply);
		}
	}
}
