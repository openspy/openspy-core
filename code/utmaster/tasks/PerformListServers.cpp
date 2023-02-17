#include "tasks.h"

#include <algorithm>
#include <server/UTPeer.h>

namespace MM {
	void LoadBasicServerInfo(UTMasterRequest request, TaskThreadData* thread_data, std::string server_key, ServerRecord &result) {

		std::vector<std::string> lookup_keys;

		lookup_keys.push_back("id");
		lookup_keys.push_back("gameid");
		lookup_keys.push_back("wan_port");
		lookup_keys.push_back("wan_ip");
		lookup_keys.push_back("mutators");

		std::string command = "HMGET " + server_key;

		std::vector<std::string>::iterator it2 = lookup_keys.begin();
		
		while (it2 != lookup_keys.end()) {
			command += " " + *it2;
			it2++;
		}

		Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, command.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
			return;

		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> > values = reply.values[0].arr_value.values;
		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> >::iterator v_it = values.begin();
		int idx = 0;
		while (v_it != values.end()) {
			std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> p = *v_it;
			v_it++;

			if (p.first != Redis::REDIS_RESPONSE_TYPE_STRING) {
				continue;
			}
			switch (idx++) {
			case 0:
				result.id = atoi(p.second.value._str.c_str());
				break;
			case 1:
				result.gameid = atoi(p.second.value._str.c_str());
				break;
			case 2:
				result.m_address.port = atoi(p.second.value._str.c_str());
				break;
			case 3:
				result.m_address.ip = inet_addr(p.second.value._str.c_str());
				break;
			case 4:
				result.m_mutators = OS::KeyStringToVector(p.second.value._str);
				break;
			}
		}
	}
	ServerRecord LoadServerInfo(UTMasterRequest request, TaskThreadData* thread_data, std::string server_key) {
		Redis::Response reply;
		Redis::Value v, arr;

		ServerRecord result;

		std::vector<std::string> lookup_keys;


		LoadBasicServerInfo(request, thread_data, server_key, result);



		//These are returned in order for parsing, first 6 are specially handled
		lookup_keys.push_back("hostname"); //0
		lookup_keys.push_back("mapname"); //1
		lookup_keys.push_back("gametype"); //2
		lookup_keys.push_back("botlevel");//3
		lookup_keys.push_back("numplayers"); //4
		lookup_keys.push_back("maxplayers");//5
		lookup_keys.push_back("GamePassword");
		lookup_keys.push_back("GameStats");
		lookup_keys.push_back("ServerVersion");
		lookup_keys.push_back("ServerMode");

		std::vector<FilterProperties>::iterator it = request.m_filters.begin();
		while (it != request.m_filters.end()) {
			FilterProperties p = *it;
			lookup_keys.push_back(p.field);
			it++;
		}

		std::string cust_keys = server_key + "custkeys";
		std::string command = "HMGET " + cust_keys;

		std::vector<std::string>::iterator it2 = lookup_keys.begin();
		while (it2 != lookup_keys.end()) {
			command += " " + *it2;
			it2++;
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, command.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
			return result;

		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> > values = reply.values[0].arr_value.values;
		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> >::iterator v_it = values.begin();
		int idx = 0;
		while (v_it != values.end()) {
			std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> p = *v_it;
			v_it++;

			if (p.first != Redis::REDIS_RESPONSE_TYPE_STRING) {
				idx++;
				continue;
			}
			std::string field_name = lookup_keys.at(idx);
			switch (idx++) {
				case 0:
					result.hostname = p.second.value._str;
					break;
				case 1:
					result.level = p.second.value._str;
					break;
				case 2:
					result.game_group = p.second.value._str;
					break;
				case 3:
					result.bot_level = p.second.value._str;
					break;
				case 4:
					result.num_players = atoi(p.second.value._str.c_str());
					break;
				case 5:
					result.max_players = atoi(p.second.value._str.c_str());
					break;
				default: //not specially managed property, dump into rules (handled outside switch due to filtering logic -- due to things like gamegroup filtering)
					break;
			}
			result.m_rules[field_name] = p.second.value._str;

		}
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
				if (!serverRecordExists(thread_data, server_key) || isServerDeleted(thread_data, server_key)) { //remove dead servers from cache
					Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", game_info.gamename.c_str(), server_key.c_str());
					continue;
				}
				ServerRecord record = LoadServerInfo(request, thread_data, server_key);
				if (filterMatches(record, request.m_filters)) {
					results.push_back(record);
				}
			}
		} while(cursor != 0);

		error_cleanup:
			return;
    }
    bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data) {
        OS::GameData game_info = request.peer->GetGameData();

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