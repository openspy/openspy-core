#include "tasks.h"

#include <algorithm>
#include <server/UTPeer.h>
#include <sstream>
namespace MM {
	bool filterMatches(ServerRecord record, std::vector<FilterProperties>& filter);

	void LoadBasicServerInfo(UTMasterRequest request, TaskThreadData* thread_data, ServerRecord& result, redisReply *reply) {
		
		for(int idx=0;idx<reply->elements;idx++) {
			if(reply->element[idx]->type != REDIS_REPLY_STRING) {
				continue;
			}

			switch (idx) {
				case 0:
					result.id = atoi(reply->element[idx]->str);
					break;
				case 1:
					result.gameid = atoi(reply->element[idx]->str);
					break;
				case 2:
					result.m_address.port = atoi(reply->element[idx]->str);
					break;
				case 3:
					result.m_address.ip = inet_addr(reply->element[idx]->str);
					break;
				case 4:
					result.m_mutators = OS::KeyStringToVector(reply->element[idx]->str);
					break;
			}
		}
	}

    void LoadServerCustomKeys(UTMasterRequest request, TaskThreadData* thread_data, MM::ServerRecord &result, redisReply *reply,const char **lookup_keys) {

        for(int idx=0;idx<reply->elements;idx++) {
            if(reply->element[idx]->type != REDIS_REPLY_STRING) {
                continue;
            }


            std::string field_name = lookup_keys[idx];
            switch (idx) {
            case 0:
                result.hostname = reply->element[idx]->str;
                break;
            case 1:
                result.level = reply->element[idx]->str;
                break;
            case 2:
                result.game_group = reply->element[idx]->str;
                break;
            case 3:
                result.bot_level = reply->element[idx]->str;
                break;
            case 4:
                result.num_players = atoi(reply->element[idx]->str);
                break;
            case 5:
                result.max_players = atoi(reply->element[idx]->str);
                break;
            default: //not specially managed property, dump into rules (handled outside switch due to filtering logic -- due to things like gamegroup filtering)
                break;
            }
            result.m_rules[field_name] = reply->element[idx]->str;
            idx++;

        }
    }


    void LoadServerFilterKeys(UTMasterRequest request, TaskThreadData* thread_data, MM::ServerRecord &result, redisReply *reply,const char **lookup_keys) {
        for(int idx=0;idx<reply->elements;idx++) {
            if(reply->element[idx]->type != REDIS_REPLY_STRING) {
                continue;
            }
            
            std::string field_name = lookup_keys[idx];
            result.m_rules[field_name] = reply->element[idx]->str;
        }
    }


	std::vector<ServerRecord> LoadServerInfo(UTMasterRequest request, TaskThreadData* thread_data, const std::vector<std::string> server_keys) {
		redisReply *reply;

		std::vector<ServerRecord> records;

		ServerRecord result;
        
        const char* basic_lookup_keys[] = {
            "HMGET", NULL,
            "id",
            "gameid",
            "wan_port",
            "wan_ip",
            "mutators"
        };
        
	
		//These are returned in order for parsing, first 6 are specially handled
        const char *lookup_keys[] = {
            "HMGET", NULL,
            "hostname",
            "mapname",
            "gametype",
            "botlevel",
            "numplayers",
            "maxplayers",
            "GamePassword",
            "GameStats",
            "ServerVersion",
            "ServerMode"
        };
        
        const char **filter_keys = (const char **)malloc((2 + request.m_filters.size()) * sizeof(const char *));
        

		std::vector<FilterProperties>::iterator itf = request.m_filters.begin();
        
        int filter_idx = 2;
        
        filter_keys[0] = "HMGET";
		while (itf != request.m_filters.end()) {
			FilterProperties p = *itf;
            filter_keys[filter_idx++] = strdup(p.field.c_str());
			itf++;
		}


		std::vector<std::string>::const_iterator its = server_keys.begin();
		while (its != server_keys.end()) {
			const std::string server_key = *its;
            std::string custkeys_key = server_key + "custkeys";
            
            basic_lookup_keys[1] = server_key.c_str();
            
            lookup_keys[1] = custkeys_key.c_str();
            filter_keys[1] = lookup_keys[1];
            
            redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(basic_lookup_keys) / sizeof(const char *), basic_lookup_keys, NULL);
            redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(lookup_keys) / sizeof(const char *), lookup_keys, NULL);
            
            if(!request.m_filters.empty()) {
                redisAppendCommandArgv(thread_data->mp_redis_connection, (2 + request.m_filters.size()), filter_keys, NULL);
            }

			its++;
		}
        
        std::vector<std::string> keys_to_delete;
        
		for (int i = 0, c = 0; i < server_keys.size(); i++, c += 2) {
			std::string server_key = server_keys.at(i);

            
			MM:ServerRecord result;

			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
			if(r == REDIS_OK) {
				LoadBasicServerInfo(request, thread_data, result, reply); //filter keys
				freeReplyObject(reply);

				r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
				if(r == REDIS_OK) {
					LoadServerCustomKeys(request, thread_data, result, reply, lookup_keys); //cust keys
					freeReplyObject(reply);
                    
                    if(!request.m_filters.empty()) {
                        r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
                        if (r == REDIS_OK) {
                            LoadServerFilterKeys(request, thread_data, result, reply, (const char **)&filter_keys[2]); //filter keys (filter_keys+2 due to initial keys)
                            freeReplyObject(reply);
                        }
                    }

					if (result.m_address.GetPort() == 0) {
                        //defer delete for later
                        keys_to_delete.push_back(server_key);
						continue;
					}

					if (filterMatches(result, request.m_filters)) {
						records.push_back(result);
					}
				}
			}
		}
        
        for(int i = 2; i < filter_idx; i++) {
            free((void *)filter_keys[i]);
        }
        free(filter_keys);
        
        //delete deferred cache misses
        std::vector<std::string>::iterator del_it = keys_to_delete.begin();
        int cmd_count = 0;
        while(del_it != keys_to_delete.end()) {
            std::string key = *del_it;
            redisAppendCommand(thread_data->mp_redis_connection, "ZREM %s %s", request.peer->GetGameData().gamename.c_str(), key.c_str());
            cmd_count++;
            del_it++;
        }
        
        for(int i=0;i<cmd_count;i++) {
            void *reply;
            int r = redisGetReply(thread_data->mp_redis_connection, (void**)&reply);
            if(r == REDIS_OK) {
                freeReplyObject(reply);
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
        redisReply *reply = NULL;	
		int cursor = 0;
        selectQRRedisDB(thread_data);
		do {

			reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d COUNT 50", game_info.gamename.c_str(), cursor);
            if (reply == NULL || thread_data->mp_redis_connection->err) {
                goto error_cleanup;
            }
			if (reply->elements < 2) {
				goto error_cleanup;
			}

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			std::vector<std::string> server_keys;
			for(size_t i=0;i<reply->element[1]->elements;i += 2) {
			 	std::string server_key = reply->element[1]->element[i]->str;
			 	server_keys.push_back(server_key);
			}
			freeReplyObject(reply);

			std::vector<MM::ServerRecord> loaded_servers = LoadServerInfo(request, thread_data, server_keys);
			results.insert(results.end(), loaded_servers.begin(), loaded_servers.end());
            
		} while(cursor != 0);
        reply = NULL;

		error_cleanup:
        if(reply != NULL) {
            freeReplyObject(reply);
        }
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
