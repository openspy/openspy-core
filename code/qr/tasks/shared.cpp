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
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}
	int TryFindServerID(TaskThreadData *thread_data, OS::Address address) {
		std::string ip = address.ToString(true);
		std::stringstream map;
		map << "IPMAP_" << ip << "-" << address.GetPort();
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", map.str().c_str());
		Redis::Value v = resp.values.front();
		int ret = -1;
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		if (ret == 1) {
			ret = -1;
			resp = Redis::Command(thread_data->mp_redis_connection, 0, "GET %s", map.str().c_str());
			v = resp.values.front();
			if(resp.values.size() > 0) {
				std::string server_key; 
				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					server_key = v.value._str;
				}

				resp = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s id", server_key.c_str());
				if(resp.values.size() > 0) {
					v = resp.values.front();
					if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						ret = v.value._int;
					}
					else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						ret = atoi(v.value._str.c_str());
					}
				}
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
		uint16_t port;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "HGET %s wan_ip", server_key.c_str());
		Redis::Value v = resp.values.front();
		 if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ip = v.value._str.c_str();
		}

		resp = Redis::Command(thread_data->mp_redis_connection, 1, "HGET %s wan_port", server_key.c_str());
		v = resp.values.front();
		 if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			port = atoi(v.value._str.c_str());
		} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			port = v.value._int;
		}


		std::stringstream ss;
		ss << ip << ":" << port;

		result = OS::Address(ss.str());
		return result;
	}
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key, bool ignoreChallengeExists) {
		std::string ip;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "HGET %s deleted", server_key.c_str());
		Redis::Value v = resp.values.front();
		 if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}

		bool challenge_exists = false;

		if(!ignoreChallengeExists) {
			resp = Redis::Command(thread_data->mp_redis_connection, 0, "HEXISTS %s challenge", server_key.c_str());

			
			if (Redis::CheckError(resp) || resp.values.size() == 0) {
				challenge_exists = true;
			} else {
				v = resp.values[0];
				if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0)) {
					challenge_exists = true;
				}
			}	
		}
		
		return ret == 1 && !challenge_exists;
	}
	int GetNumHeartbeats(TaskThreadData *thread_data, std::string server_key) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = 0;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "HGET %s num_updates", server_key.c_str());
		Redis::Value v = resp.values.front();
		 if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		return ret;
	}
}