#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {
    bool PerformGetServers(MMQueryRequest request, TaskThreadData *thread_data) {
			GetServers(thread_data, &request);
			if(request.peer)
				request.peer->DecRef();
			return true;
    }

	void GetServers(TaskThreadData *thread_data, const MMQueryRequest *request) {
		ServerListQuery ret;
		redisReply* reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
		if(!reply) {
			return;
		}
		freeReplyObject(reply);

		int cursor = 0;
		bool sent_servers = false;
        
        int peer_version = 0;

        if(request && request->peer) {
            peer_version = request->peer->GetVersion();
        }
		do {
			ServerListQuery streamed_ret;
			streamed_ret.requested_fields = request->req.field_list;

			std::string for_gamename = OS::str_tolower(request->req.m_for_game.gamename);
			reply =  (redisReply *) redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d COUNT 50", for_gamename.c_str(), cursor);

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


			AppendServerEntries(thread_data, server_keys, &streamed_ret, &request->req, peer_version);

			if(cursor == 0) {
				streamed_ret.last_set = true;
			}

			if (request && (!streamed_ret.list.empty() || streamed_ret.last_set)) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				request->peer->OnRetrievedServers(*request, streamed_ret, request->extra);
			}

			FreeServerListQuery(&streamed_ret);

			freeReplyObject(reply);
		} while(cursor != 0);
        reply = NULL;

		error_cleanup:
        if(reply != NULL) {
            freeReplyObject(reply);
        }
        return;
	}

	void BuildServerListRequestData(const sServerListReq* req, std::vector<std::string>& basic_lookup_keys, std::vector<std::string>& lookup_keys, std::vector<CToken> &out_token_list, int peer_version) {
		std::vector<CToken> token_list;

		if (req != NULL) {
			token_list = CToken::filterToTokenList(req->filter.c_str());
			out_token_list.insert(out_token_list.end(), token_list.begin(), token_list.end());
		}

		basic_lookup_keys.push_back("deleted");
		basic_lookup_keys.push_back("gameid");
		basic_lookup_keys.push_back("id");
		basic_lookup_keys.push_back("wan_port");
		basic_lookup_keys.push_back("wan_ip");
		basic_lookup_keys.push_back("icmp_address");
		basic_lookup_keys.push_back("allow_unsolicited_udp");
		basic_lookup_keys.push_back("country");

		if (req == NULL) {
			return;
		}

		std::vector<std::string>::const_iterator field_it = req->field_list.begin();
		while (field_it != req->field_list.end()) {
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
        
        if(peer_version == 2) {
            const char *injected_custom_keys[] = {"localport", "localip0", "natneg"};
            for(int i=0;i<sizeof(injected_custom_keys) / sizeof(const char *);i++) {
                lookup_keys.push_back(injected_custom_keys[i]);
            }
        }

	}

	void AppendServerEntries_AllKeys(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req) {
		std::vector<std::string> basic_lookup_keys;

		std::vector<std::string> lookup_keys;

		std::vector<CToken> token_list;

		redisReply *reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
		freeReplyObject(reply);

		BuildServerListRequestData(req, basic_lookup_keys, lookup_keys, token_list, 0);

		std::ostringstream cmds;
        
        std::vector<std::string> keys_to_delete;

		std::vector<std::string>::iterator it = server_keys.begin();
		while (it != server_keys.end()) {
			std::string server_key = *it;
            
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

			std::string custkeys_cmd = "HGETALL " + server_key + "custkeys";
			redisAppendCommand(thread_data->mp_redis_connection, custkeys_cmd.c_str());
			it++;
		}

        for (size_t c = 0; c < server_keys.size(); c++) {
            Server server;
            
            redisReply *basic_reply = NULL;
            redisReply *custom_reply = NULL;
            
            redisGetReply(thread_data->mp_redis_connection,(void**)&basic_reply);
            redisGetReply(thread_data->mp_redis_connection,(void**)&custom_reply);
            
            std::string server_key = server_keys.at(c);
            
            if (!LoadBasicServerInfo(&server, basic_reply)) {
                keys_to_delete.push_back(server_key);
                
                if(basic_reply != NULL) {
                    freeReplyObject(basic_reply);
                }
                if(custom_reply != NULL) {
                    freeReplyObject(custom_reply);
                }
                continue;
            }
            
            //skip deleted server
            if (!req->include_deleted && server.deleted) {
                if(basic_reply != NULL) {
                    freeReplyObject(basic_reply);
                }
                if(custom_reply != NULL) {
                    freeReplyObject(custom_reply);
                }
                continue;
            }
            LoadCustomServerInfo_AllKeys(&server, custom_reply);
            server.kvFields["backend_id"] = server_key;
            server.key = server_key;
            
            if(basic_reply != NULL) {
                freeReplyObject(basic_reply);
            }
            if(custom_reply != NULL) {
                freeReplyObject(custom_reply);
            }
            
            if (filterMatches(token_list, server.kvFields)) {
                if (query_response->list.size() < req->max_results || req->max_results == 0) {
                    
                    //only load this data if its actually being sent
                    if (req->all_player_keys) {
                        LoadCustomInfo_AllPlayerKeys(thread_data, &server, server_key); //this is SLOW (it performs queries directly)
                    }
                    
                    if (req->all_team_keys) {
                        LoadCustomInfo_AllTeamKeys(thread_data, &server, server_key); //this is SLOW (it performs queries directly)
                    }
                    
                    
                    Server* _server = new Server();
                    *_server = server;
                    query_response->list.push_back(_server);
                }
            }
        }
        
		if(!req->m_for_game.gamename.empty()) {
			std::vector<std::string>::iterator del_it = keys_to_delete.begin();
			while(del_it != keys_to_delete.end()) {
				std::string server_key = *del_it;
				std::string gamename =  req->m_for_game.gamename; //incase of reference issue
				const char *args[] = {"ZREM" , gamename.c_str(), server_key.c_str()};
				redisAppendCommandArgv(thread_data->mp_redis_connection, 3, args, NULL);
				del_it++;
			}
			
			for(size_t i=0;i<keys_to_delete.size();i++) {
				void *reply;
				int r = redisGetReply(thread_data->mp_redis_connection, (void **)&reply);
				if (r == REDIS_OK) {
					freeReplyObject(reply);
				}
			}
		}
	}

	void LoadCustomInfo_AllPlayerKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key) {
		int idx = 0;

		redisReply *reply = NULL;

		do {
			std::ostringstream s;
			s << server_key << "custkeys_player_" << idx;

			std::string key = s.str();

			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS %s", key.c_str());

           if (reply == NULL || thread_data->mp_redis_connection->err) {
				if(reply != NULL) {
					freeReplyObject(reply);
				}
                break;
            }

			if(reply->type == REDIS_REPLY_STRING) {
				if (!atoi(reply->str)) {
					freeReplyObject(reply);
					break;
				}					
			} else if(reply->type == REDIS_REPLY_INTEGER) {
				if(!reply->integer) {
					freeReplyObject(reply);
					break;
				}
			}


			idx++;
		} while (true);

		if (idx == 0) {
			return;
		}

		//we got the max player key, now dump them
		for (int i = 0; i < idx; i++) {
			std::ostringstream cmd;
			cmd << "HGETALL " << server_key << "custkeys_player_" << i;
			redisAppendCommand(thread_data->mp_redis_connection, cmd.str().c_str());
		}

		

		for (int i = 0, x = 0; i < idx; i++) {
			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);

			if(r == REDIS_OK) {
				for(size_t c = 0; c < reply->elements; c += 2) {
					std::string field_name = reply->element[c]->str;
					std::string field_value = reply->element[c + 1]->str;
					server->kvPlayers[x][field_name] = field_value;
				}
				x++;
				freeReplyObject(reply);
			}

		}
	}

	void LoadCustomInfo_AllTeamKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key) {
		int idx = 0;

		redisReply *reply = NULL;

		do {
			std::ostringstream s;
			s << server_key << "custkeys_team_" << idx;

			std::string key = s.str();

			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXISTS %s", key.c_str());

           if (reply == NULL || thread_data->mp_redis_connection->err) {
				if(reply != NULL) {
					freeReplyObject(reply);
				}
                break;
            }

			if(reply->type == REDIS_REPLY_STRING) {
				if (!atoi(reply->str)) {
					freeReplyObject(reply);
					break;
				}					
			} else if(reply->type == REDIS_REPLY_INTEGER) {
				if(!reply->integer) {
					freeReplyObject(reply);
					break;
				}
			}


			idx++;
		} while (true);

		if (idx == 0) {
			return;
		}

		//we got the max player key, now dump them
		for (int i = 0; i < idx; i++) {
			std::ostringstream cmd;
			cmd << "HGETALL " << server_key << "custkeys_team_" << i;
			redisAppendCommand(thread_data->mp_redis_connection, cmd.str().c_str());
		}

		for (int i = 0, x = 0; i < idx; i++) {
			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);

			if(r == REDIS_OK) {
				for(size_t c = 0; c < reply->elements; c += 2) {
					std::string field_name = reply->element[c]->str;
					std::string field_value = reply->element[c + 1]->str;
					server->kvTeams[x][field_name] = field_value;
				}
				x++;
				
				freeReplyObject(reply);
			}
		}
	}

	void AppendServerEntries(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req, int peer_version) {
		std::vector<std::string> basic_lookup_keys;

		std::vector<std::string> lookup_keys;

		std::vector<CToken> token_list;

		if (server_keys.empty()) {
			return;
		}

		
		if (req->all_keys) {
			AppendServerEntries_AllKeys(thread_data, server_keys, query_response, req);
			return;
		}
		
		BuildServerListRequestData(req, basic_lookup_keys, lookup_keys, token_list, peer_version);

		bool has_customkeys = !lookup_keys.empty();

        std::vector<std::string> keys_to_delete;
		std::vector<std::string>::iterator it = server_keys.begin();
		while (it != server_keys.end()) {
			std::string server_key = *it;
            
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

			if (has_customkeys) {
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
			it++;
		}


		it = server_keys.begin();
		while (it != server_keys.end()) {
			Server server;

			std::string server_key = *it;

			redisReply *basic_reply = NULL;
			redisReply *custom_reply = NULL;

			redisGetReply(thread_data->mp_redis_connection,(void**)&basic_reply);
            if (has_customkeys) {
                redisGetReply(thread_data->mp_redis_connection,(void**)&custom_reply);
            }

			if (!LoadBasicServerInfo(&server, basic_reply)) {
                
				//server is deleted / doesn't exist, remove from cache
				if (req) {
                    keys_to_delete.push_back(server_key);
				}
			}
			else if(req->include_deleted || server.deleted == false) {
				if (has_customkeys && custom_reply) {
					LoadCustomServerInfo(&server, lookup_keys, custom_reply);
				}
				server.kvFields["backend_id"] = server_key;
                server.key = server_key;
                server.game = req->m_for_game;

				if (!req || filterMatches(token_list, server.kvFields)) {
					if (!req || (query_response->list.size() < req->max_results || req->max_results == 0)) {
						Server* _server = new Server();
						*_server = server;
						query_response->list.push_back(_server);
					}
				}
			}
            
			if(basic_reply != NULL) {
				freeReplyObject(basic_reply);
			}
			if(custom_reply != NULL) {
				freeReplyObject(custom_reply);
			}
			
			it++;
		}
        
        std::vector<std::string>::iterator del_it = keys_to_delete.begin();
        while(del_it != keys_to_delete.end()) { //delete cache misses
            std::string server_key = *del_it;
            std::string gamename =  req->m_for_game.gamename; //incase of reference issue
            const char *args[] = {"ZREM" , gamename.c_str(), server_key.c_str()};
            redisAppendCommandArgv(thread_data->mp_redis_connection, 3, args, NULL);
            del_it++;
        }
        for(size_t i=0;i<keys_to_delete.size();i++) {
            void *reply;
            int r = redisGetReply(thread_data->mp_redis_connection, (void **)&reply);
            if (r == REDIS_OK) {
                freeReplyObject(reply);
            }
        }
	}
	
	void LoadCustomServerInfo_AllKeys(MM::Server* server, redisReply* custom_keys_response) {
		for (size_t i = 0; i < custom_keys_response->elements; i += 2) {
			std::string field_name = custom_keys_response->element[i]->str;
			std::string field_value = custom_keys_response->element[i + 1]->str;
			server->kvFields[field_name] = field_value;
		}
	}

	void LoadCustomServerInfo(MM::Server* server, std::vector<std::string>& custom_lookup_keys, redisReply* custom_keys_response) {
		for (size_t i = 0; i < custom_keys_response->elements; i ++) {
			if(custom_keys_response->element[i]->type != REDIS_REPLY_STRING) {
				continue;
			}
			std::string field_name = custom_lookup_keys.at(i);
			std::string field_value = custom_keys_response->element[i]->str;
			server->kvFields[field_name] = field_value;
		}
	}

	bool LoadBasicServerInfo(MM::Server* server, redisReply* basic_keys_response) {
        if(!basic_keys_response) {
            return false;
        }
		for(size_t i = 0; i < basic_keys_response->elements; i++) {

			if (basic_keys_response->element[i]->type != REDIS_REPLY_STRING) {
				continue;
			}
			switch (i) {
			case 0:
				if (basic_keys_response->element[i]->type != REDIS_REPLY_NIL) {
					if (atoi(basic_keys_response->element[i]->str) != 0) {
						//mark as deleted (we only remove non-existent from the cache)
						server->deleted = true;
					}
				}
				break;
			case 1:
				server->game.gameid = atoi(basic_keys_response->element[i]->str);
				break;
			case 2:
				server->id = atoi(basic_keys_response->element[i]->str);
				break;
			case 3:
				server->wan_address.port = atoi(basic_keys_response->element[i]->str);
				break;
			case 4:
				server->wan_address.ip = inet_addr(basic_keys_response->element[i]->str);
				break;
			case 5:
				server->icmp_address.ip = inet_addr(basic_keys_response->element[i]->str);
				break;
			case 6:
				server->allow_unsolicited_udp = atoi(basic_keys_response->element[i]->str);
				break;
			case 7:
				server->kvFields["country"] = basic_keys_response->element[i]->str;
				break;

			}
		}

		//if the ip is null its an invalid server
		if (server->wan_address.ip == 0) {
			return false;
		}
		return true;
	}
}
