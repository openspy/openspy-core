#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {
    void AppendGroupEntries_AllKeys(TaskThreadData* thread_data, redisReply *keys_array, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list);
	void AppendGroupEntries(TaskThreadData* thread_data, redisReply *keys_array, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list);
	void LoadBasicGroupInfo(MM::Server* server, redisReply *basic_keys_response);
    void LoadCustomServerInfo(MM::Server* server, std::vector<std::string>& custom_lookup_keys, redisReply* custom_keys_response);
    void LoadCustomServerInfo_AllKeys(MM::Server* server, redisReply* custom_keys_response);

    bool PerformGetGroups(MMQueryRequest request, TaskThreadData *thread_data) {
        GetGroups(thread_data, &request); //no need to free, because nothing appended to return value
		if(request.peer)
			request.peer->DecRef();
        return true;
    }

	void GetGroups(TaskThreadData *thread_data, const MMQueryRequest *request) {
		redisReply *reply = NULL;	
		int cursor = 0;
		bool sent_servers = false;

		reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_SBGroups);
		if(reply) {
			freeReplyObject(reply);
		}
		

		std::vector<CToken> token_list = CToken::filterToTokenList(request->req.filter.c_str());
		do {
			ServerListQuery streamed_ret;
			reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d COUNT 50", request->req.m_for_game.gamename.c_str(), cursor);
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

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}

			if (request->req.all_keys) {
                AppendGroupEntries_AllKeys(thread_data, reply->element[1], &streamed_ret, request, token_list);
			}
			else {
				AppendGroupEntries(thread_data, reply->element[1], &streamed_ret, request, token_list);
			}

			if (request && (streamed_ret.last_set || !streamed_ret.list.empty())) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				request->peer->OnRetrievedGroups(*request, streamed_ret, request->extra);
			}
			FreeServerListQuery(&streamed_ret);

		} while(cursor != 0);

        reply = NULL;

		error_cleanup:
        if(reply != NULL) {
            freeReplyObject(reply);
        }
	}

    void AppendGroupEntries_AllKeys(TaskThreadData* thread_data, redisReply *keys_array, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list) {
        redisReply *reply = NULL;
        if (keys_array == NULL || keys_array->elements == 0) {
            return;
        }

        std::vector<std::string> basic_lookup_keys;

        basic_lookup_keys.push_back("gameid");
        basic_lookup_keys.push_back("groupid");
        basic_lookup_keys.push_back("maxwaiting");
        basic_lookup_keys.push_back("hostname");
        basic_lookup_keys.push_back("password");
        basic_lookup_keys.push_back("numwaiting");
        basic_lookup_keys.push_back("numservers");

        std::vector<std::string> lookup_keys;

        std::vector<std::string>::const_iterator field_it = request->req.field_list.begin();
        while (field_it != request->req.field_list.end()) {
            const std::string field = *field_it;
            lookup_keys.push_back(field);
            field_it++;
        }

        std::vector<CToken>::iterator itf = token_list.begin();
        while (itf != token_list.end()) {
            CToken token = *itf;
            if (token.getType() == EToken_Variable) {
                lookup_keys.push_back(token.getString());
            }
            itf++;
        }

        std::ostringstream cmds;
        for (size_t i = 0; i < keys_array->elements; i += 2) {
            std::string server_key = keys_array->element[i]->str;
            
            int num_basic_keys = 2 + basic_lookup_keys.size();
            const char **args = (const char **)malloc(num_basic_keys * sizeof(const char *));
            args[0] = "HMGET";
            args[1] = server_key.c_str();
            
            int idx = 2;
            
            std::vector<std::string>::iterator keys_it = basic_lookup_keys.begin();
            while (keys_it != basic_lookup_keys.end()) {
                std::string key = *keys_it;
                args[idx++] = strdup(key.c_str());
                keys_it++;
            }
            
            redisAppendCommandArgv(thread_data->mp_redis_connection, idx, args, NULL);
            for(int i=2;i<idx;i++) {
                free((void *)args[i]);
            }
            free(args);
            
            std::string server_cust_key = server_key + "custkeys";
            redisAppendCommand(thread_data->mp_redis_connection, "HGETALL %s", server_cust_key.c_str());
        }


        for (size_t i = 0; i < keys_array->elements; i += 2) {
            Server server;

            int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
            
            bool success = true;
            if(r == REDIS_OK) {
                LoadBasicGroupInfo(&server, reply);
                freeReplyObject(reply);
            } else success = false;

            r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
            
            if(r == REDIS_OK) {
                LoadCustomServerInfo_AllKeys(&server, reply);
                freeReplyObject(reply);
            } else success = false;


            if (success && (!request || filterMatches(token_list, server.kvFields))) {
                if (!request || (query_response->list.size() < request->req.max_results || request->req.max_results == 0)) {
                    Server* _server = new Server();
                    *_server = server;
                    query_response->list.push_back(_server);
                }
            }
        }
    }

	void AppendGroupEntries(TaskThreadData* thread_data, redisReply *keys_array, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list) {
		redisReply *reply = NULL;
		if (keys_array == NULL || keys_array->elements == 0) {
			return;
		}

		std::vector<std::string> basic_lookup_keys;

		basic_lookup_keys.push_back("gameid");
		basic_lookup_keys.push_back("groupid");
		basic_lookup_keys.push_back("maxwaiting");
		basic_lookup_keys.push_back("hostname");
		basic_lookup_keys.push_back("password");
		basic_lookup_keys.push_back("numwaiting");
		basic_lookup_keys.push_back("numservers");

		std::vector<std::string> lookup_keys;

		std::vector<std::string>::const_iterator field_it = request->req.field_list.begin();
		while (field_it != request->req.field_list.end()) {
			const std::string field = *field_it;
			lookup_keys.push_back(field);
			field_it++;
		}

		std::vector<CToken>::iterator itf = token_list.begin();
		while (itf != token_list.end()) {
			CToken token = *itf;
			if (token.getType() == EToken_Variable) {
				lookup_keys.push_back(token.getString());
			}
			itf++;
		}

		std::ostringstream cmds;
		for (size_t i = 0; i < keys_array->elements; i += 2) {
			std::string server_key = keys_array->element[i]->str;
            
            int num_basic_keys = 2 + basic_lookup_keys.size();
            const char **args = (const char **)malloc(num_basic_keys * sizeof(const char *));
            args[0] = "HMGET";
            args[1] = server_key.c_str();
            
            int idx = 2;
            
            std::vector<std::string>::iterator keys_it = basic_lookup_keys.begin();
            while (keys_it != basic_lookup_keys.end()) {
                std::string key = *keys_it;
                args[idx++] = strdup(key.c_str());
                keys_it++;
            }
            
            redisAppendCommandArgv(thread_data->mp_redis_connection, idx, args, NULL);
            for(int i=2;i<idx;i++) {
                free((void *)args[i]);
            }
            free(args);
            
            std::string server_cust_key = server_key + "custkeys";
            int num_cust_keys = 2 + lookup_keys.size();
            idx = 2;
            args = (const char **)malloc(num_cust_keys * sizeof(const char *));
            args[0] = "HMGET";
            args[1] = server_cust_key.c_str();
            
            keys_it = lookup_keys.begin();
            while (keys_it != lookup_keys.end()) {
                std::string key = *keys_it;
                args[idx++] = strdup(key.c_str());
                keys_it++;
            }
            
            redisAppendCommandArgv(thread_data->mp_redis_connection, idx, args, NULL);
            
            for(int i=2;i<idx;i++) {
                free((void *)args[i]);
            }
            free(args);
		}


		for (size_t i = 0; i < keys_array->elements; i += 2) {
			Server server;

			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
            
            bool success = true;
            if(r == REDIS_OK) {
                LoadBasicGroupInfo(&server, reply);
                freeReplyObject(reply);
            } else success = false;

			r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
            
            if(r == REDIS_OK) {
                LoadCustomServerInfo(&server, lookup_keys, reply);
                freeReplyObject(reply);
            } else success = false;


			if (success && (!request || filterMatches(token_list, server.kvFields))) {
				if (!request || (query_response->list.size() < request->req.max_results || request->req.max_results == 0)) {
					Server* _server = new Server();
					*_server = server;
					query_response->list.push_back(_server);
				}
			}
		}
	}

	void LoadBasicGroupInfo(MM::Server* server, redisReply *basic_keys_response) {
		for(size_t i = 0; i<basic_keys_response->elements; i++) {

			if(basic_keys_response->element[i]->type != REDIS_REPLY_STRING) {
				continue;
			}
			switch(i) {
				case 0:
					server->game.gameid = atoi(basic_keys_response->element[i]->str);
				break;
				case 1:
					server->wan_address.ip = htonl(atoi((basic_keys_response->element[i]->str))); //for V2
					server->kvFields["groupid"] = basic_keys_response->element[i]->str; //for V1
				break;
				case 2:
					server->kvFields["maxwaiting"] = basic_keys_response->element[i]->str;
				break;
				case 3:
					server->kvFields["hostname"] = basic_keys_response->element[i]->str;
				break;
				case 4:
					server->kvFields["password"] = basic_keys_response->element[i]->str;
				break;
				case 5:
					server->kvFields["numwaiting"] = basic_keys_response->element[i]->str;
				break;
				case 6:
					server->kvFields["numservers"] = basic_keys_response->element[i]->str;
				break;
			}
		}
	}
}
