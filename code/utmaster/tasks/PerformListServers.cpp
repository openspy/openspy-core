#include "tasks.h"

#include <algorithm>
#include <server/UTPeer.h>

namespace MM {
	std::string GetHKey(TaskThreadData *thread_data, std::string server_key, std::string key) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s %s", server_key.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return "";

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			return v.value._str;
		return "";
	}
	int GetHKeyInt(TaskThreadData *thread_data, std::string server_key, std::string key) {
		return atoi(GetHKey(thread_data, server_key, key).c_str());
	}
	int GetPortFromGamespyInfo(TaskThreadData *thread_data, std::string server_key) {
		Redis::Response resp;
		Redis::Value v, arr;

		bool is_gamespy = false;

		resp = Redis::Command(thread_data->mp_redis_connection, 0, "HEXISTS %s instance_key", server_key.c_str());

		if (Redis::CheckError(resp) || resp.values.size() == 0) {
			is_gamespy = false;
		} else {
			v = resp.values[0];
			if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0)) {
				is_gamespy = true;
			}
		}

		if(is_gamespy) {
			std::string cust_keys = server_key + "custkeys";
			return GetHKeyInt(thread_data, cust_keys, "hostport");
		}


		return 0;
	}
    ServerRecord LoadServerInfo(UTMasterRequest request, TaskThreadData *thread_data, std::string server_key) {
		Redis::Response reply;
		Redis::Value v, arr;

        ServerRecord result;
		
		result.id = GetHKeyInt(thread_data, server_key, "id");
		result.gameid = GetHKeyInt(thread_data, server_key, "gameid");

		//check if its from gamespy query
		int port = GetPortFromGamespyInfo(thread_data, server_key);
		if(port == 0)
			port = GetHKeyInt(thread_data, server_key, "wan_port");

		result.m_address.port = port;
		result.m_address.ip = inet_addr(GetHKey(thread_data, server_key, "wan_ip").c_str());

		//check if its a gamespy query connection, update wan port if so

		std::string mutators = GetHKey(thread_data, server_key, "mutators");
		if(mutators.length() > 0) {
			result.m_mutators = OS::KeyStringToVector(mutators);
		}

		std::string cust_keys = server_key + "custkeys";

		result.hostname = GetHKey(thread_data, cust_keys, "hostname");
		result.level = GetHKey(thread_data, cust_keys, "mapname");
		result.game_group = GetHKey(thread_data, cust_keys, "gametype");
		result.num_players = GetHKeyInt(thread_data, cust_keys, "numplayers");
		result.max_players = GetHKeyInt(thread_data, cust_keys, "maxplayers");

		//XXX: iterate cust keys

		int cursor = 0;

		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %s %d MATCH *", cust_keys.c_str(), cursor);
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
				goto error_cleanup;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
				if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				} else if(v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.arr_value.values[0].second.value._int;
				}

				for(size_t i=0;i<arr.arr_value.values.size();i+=2) {

					if(arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					std::string key = arr.arr_value.values[i].second.value._str;
					std::string value;
					if (arr.arr_value.values[i+1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
						value = (arr.arr_value.values[i + 1].second.value._str);
					}
					result.m_rules[key] = value;
				}
			}
		} while (cursor != 0);
		error_cleanup:
        return result;
    }

	bool hasMutator(ServerRecord record, std::string mutator) {
		std::vector<std::string>::iterator it = std::find(record.m_mutators.begin(), record.m_mutators.end(), mutator);
		return it != record.m_mutators.end();
	}
	bool filterPropertyMatches(ServerRecord record, FilterProperties property) {
		bool match = false;
		if(stricmp(property.field.c_str(), "mutator") == 0) {
			match = hasMutator(record, property.property);
		} else {
			
			std::map<std::string, std::string>::iterator rule = record.m_rules.find(property.field);
			if(rule == record.m_rules.end()) {
				match = false;
			}
			else {
				std::pair<std::string, std::string> p = *rule;
				if(strcasecmp(property.property.c_str(), p.second.c_str()) == 0) {
					match = true;
				}
			}
		}
		if(property.is_negate) {
			match = !match;
		}
		return match;
	}
	bool filterMatches(ServerRecord record, std::vector<FilterProperties> &filter) {
		std::vector<FilterProperties>::iterator it = filter.begin();
		while(it != filter.end()) {
			if(!filterPropertyMatches(record, *it)) {
				return false;
			}
			it++;
		}
		return true;
	}
    void GetServers(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, std::vector<ServerRecord> &results) {
		Redis::Response reply;
		Redis::Value v, arr;


		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		
		int cursor = 0;
		do {

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", game_info.gamename.c_str(), cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

			if (reply.values[0].arr_value.values.size() < 2) {
				goto error_cleanup;
			}
			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

		 	if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
		 		cursor = atoi(v.value._str.c_str());
		 	} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
		 		cursor = v.value._int;
		 	}

			for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
				if (!serverRecordExists(thread_data, server_key)) { //remove dead servers from cache
					Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", game_info.gamename.c_str(), server_key.c_str());
					continue;
				}
				if(!isServerDeleted(thread_data, server_key)) {
                	ServerRecord record = LoadServerInfo(request, thread_data, server_key);				
					if(filterMatches(record, request.m_filters)) {
						results.push_back(record);
						break;
					}					
				}
			}
		} while(cursor != 0);

		error_cleanup:
			return;
    }
    bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data) {
        OS::GameData game_info = OS::GetGameByID(request.peer->GetGameId(), thread_data->mp_redis_connection);

        MMTaskResponse response;
        response.peer = request.peer;

        std::vector<ServerRecord> results;

        GetServers(request, thread_data, game_info, results);
		response.server_records = results;

        if(request.peer) {
            request.peer->DecRef();
        }
        if(request.callback) {
            request.callback(response);
        }
        return true;
    }
}