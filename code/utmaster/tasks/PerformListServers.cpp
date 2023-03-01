#include "tasks.h"

#include <algorithm>
#include <server/UTPeer.h>
#include <sstream>
namespace MM {
	bool filterMatches(ServerRecord record, std::vector<FilterProperties>& filter);
	void LoadBasicServerInfo(UTMasterRequest request, TaskThreadData* thread_data, ServerRecord& result, std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> > values) {
		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> >::iterator v_it = values.begin();
		int idx = 0;
		while (v_it != values.end()) {
			std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> p = *v_it;
			v_it++;

			if (p.first != Redis::REDIS_RESPONSE_TYPE_STRING) {
				idx++;
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

	void LoadServerCustomKeys(UTMasterRequest request, TaskThreadData* thread_data, MM::ServerRecord &result, std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> > values, std::vector<std::string> lookup_keys) {
		Redis::Response reply;
		Redis::Value v, arr;

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
	}

	std::vector<ServerRecord> LoadServerInfo(UTMasterRequest request, TaskThreadData* thread_data, const std::vector<std::string> server_keys) {
		std::vector<ServerRecord> records;
		Redis::Response reply;
		Redis::Value v, arr;

		ServerRecord result;

		std::string basic_lookup_str;

		std::vector<std::string> basic_lookup_keys;
		basic_lookup_keys.push_back("id");
		basic_lookup_keys.push_back("gameid");
		basic_lookup_keys.push_back("wan_port");
		basic_lookup_keys.push_back("wan_ip");
		basic_lookup_keys.push_back("mutators");

		std::vector<std::string>::iterator it = basic_lookup_keys.begin();
		while (it != basic_lookup_keys.end()) {
			basic_lookup_str += " " + *it;
			it++;
		}

		std::string lookup_str;
		std::vector<std::string> lookup_keys;
		
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

		it = lookup_keys.begin();

		while (it != lookup_keys.end()) {
			lookup_str += " " + *it;
			it++;
		}

		std::vector<FilterProperties>::iterator itf = request.m_filters.begin();
		while (itf != request.m_filters.end()) {
			FilterProperties p = *itf;
			lookup_keys.push_back(p.field);
			lookup_str += " " + p.field;
			itf++;
		}



		std::ostringstream cmds;


		std::vector<std::string>::const_iterator its = server_keys.begin();
		while (its != server_keys.end()) {
			const std::string server_key = *its;
			cmds << "HMGET " << server_key << basic_lookup_str << "\r\n";
			cmds << "HMGET " << server_key << "custkeys" << lookup_str << "\r\n";
			its++;
		}

		reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());

		for (int i = 0, c = 0; i < server_keys.size(); i++, c += 2) {
			Redis::Value basic_keys = reply.values.at(c);
			Redis::Value custom_keys = reply.values.at(c+1);

			std::string server_key = server_keys.at(i);

			MM:ServerRecord result;

			LoadBasicServerInfo(request, thread_data, result, basic_keys.arr_value.values);

			//Checking port because id is not set yet it seems...
			if (result.m_address.GetPort() == 0) {
				Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", request.peer->GetGameData().gamename.c_str(), server_key.c_str());
				continue;
			}
			LoadServerCustomKeys(request, thread_data, result, custom_keys.arr_value.values, lookup_keys);
			if (filterMatches(result, request.m_filters)) {
				records.push_back(result);
			}
		}

		return records;
    }

	bool hasMutator(ServerRecord record, std::string mutator) {
		std::vector<std::string>::iterator it = std::find(record.m_mutators.begin(), record.m_mutators.end(), mutator);
		return it != record.m_mutators.end();
	}
	bool filterPropertyMatches(ServerRecord record, FilterProperties property) {
		bool string_comparision = false;

		switch (property.type) {
		case QT_Equals:
		case QT_NotEquals:
			string_comparision = true;
			break;
		}
		
		std::map<std::string, std::string>::iterator rule = record.m_rules.find(property.field);
		if (rule == record.m_rules.end()) {
			return true;
		}
		std::pair<std::string, std::string> p = *rule;

		int comparison = 0;

		if (stricmp(property.field.c_str(), "mutator") == 0) {
			if (!hasMutator(record, property.property)) {
				comparison = 1;
			}
		} else if (stricmp(property.field.c_str(), "nomutators") == 0) {
			if(record.m_mutators.empty()) {
				comparison = 1;
			}
		}
		else if (string_comparision) {
			comparison = strcasecmp(property.property.c_str(), p.second.c_str());
		}
		else {
			int values[2];
			values[1] = atoi(property.property.c_str());
			values[0] = atoi(p.second.c_str());

			if (values[0] == values[1]) {
				comparison = 0;
			}
			else if (values[0] < values[1]) {
				comparison = -1;
			}
			else if (values[0] > values[1]) {
				comparison = 1;
			}
		}

		switch (property.type) {
		case QT_Equals:
			if (comparison == 0) {
				return true;
			}
			break;
		case QT_NotEquals:
			if (comparison != 0) {
				return true;
			}
			break;
		case QT_LessThan:
			if (comparison < 0) {
				return true;
			}
			break;
		case QT_LessThanEquals:
			if (comparison <= 0) {
				return true;
			}
			break;
		case QT_GreaterThan:
			if (comparison > 0) {
				return true;
			}
			break;
		case QT_GreaterThanEquals:
			if (comparison >= 0) {
				return true;
			}
			break;

		}
		return false;
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

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d COUNT 50", game_info.gamename.c_str(), cursor);
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

			std::vector<std::string> server_keys;
			for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
				server_keys.push_back(server_key);
			}

			std::vector<MM::ServerRecord> loaded_servers = LoadServerInfo(request, thread_data, server_keys);
			results.insert(results.end(), loaded_servers.begin(), loaded_servers.end());
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