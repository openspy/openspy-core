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
	int TryFindServerID(TaskThreadData *thread_data, ServerInfo server) {
		std::string ip = server.m_address.ToString(true);
		std::stringstream map;
		map << "IPMAP_" << ip << "-" << server.m_address.GetPort();
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
}