#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {
    bool PerformGetServers(MMQueryRequest request, TaskThreadData *thread_data) {
			GetServers(thread_data, &request.req, &request);
			if(request.peer)
				request.peer->DecRef();
			return true;
    }

	ServerListQuery GetServers(TaskThreadData *thread_data, const sServerListReq *req, const MMQueryRequest *request) {
		ServerListQuery ret;

		Redis::Response reply;
		Redis::Value v, arr;

		ret.requested_fields = req->field_list;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		
		int cursor = 0;
		bool sent_servers = false;

		do {
			ServerListQuery streamed_ret;
			streamed_ret.requested_fields = ret.requested_fields;
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", req->m_for_game.gamename.c_str(), cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

			streamed_ret.last_set = false;
			if (cursor == 0) {
				streamed_ret.first_set = true;
			} else {
				streamed_ret.first_set = false;
			}
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

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}

			for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", server_key.c_str());
				
				if (Redis::CheckError(reply) || reply.values.size() == 0) {
					continue;
				}
				else {
					v = reply.values[0];
					if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 0) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("0") == 0)) {
						Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", req->m_for_game.gamename.c_str(), server_key.c_str());
						continue;
					}
				}

				if (request) {
					AppendServerEntry(thread_data, server_key, &streamed_ret, req->all_keys, false, req);
				}
				else {
					AppendServerEntry(thread_data, server_key, &ret, req->all_keys, false, req);
				}
			}
			if (request && (!streamed_ret.list.empty() || streamed_ret.last_set)) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				//printf("Add server req first: %d last: %d numservmp_redis_connections: %d, cursor: %d\n", streamed_ret.first_set, streamed_ret.last_set, streamed_ret.list.size(), cursor);
				request->peer->OnRetrievedServers(*request, streamed_ret, request->extra);
			}

			FreeServerListQuery(&streamed_ret);
		} while(cursor != 0);

		error_cleanup:
			return ret;
	}
	
	void FetchServerAllKeys(TaskThreadData* thread_data, std::string entry_name, Server* server, ServerListQuery* ret) {
		Redis::Response reply;
		Redis::Value v, arr;
		int cursor = 0;
		int idx = 0;

		std::string key;
		std::ostringstream s;


		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %scustkeys %d MATCH *", entry_name.c_str(), cursor);
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
				return;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.arr_value.values[0].second.value._int;
				}

				for (size_t i = 0; i < arr.arr_value.values.size(); i += 2) {

					if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					std::string key = arr.arr_value.values[i].second.value._str;
					if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
						server->kvFields[key] = (arr.arr_value.values[i + 1].second.value._str);
					}
					else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						server->kvFields[key] = arr.arr_value.values[i + 1].second.value._int;
					}

					if (std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), key) == ret->captured_basic_fields.end()) {
						ret->captured_basic_fields.push_back(key);
					}
				}
			}
		} while (cursor != 0);

		cursor = 0;
		do {
			s << entry_name << "custkeys_player_" << idx;
			key = s.str();

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", key.c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				break;

			v = reply.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				if (!v.value._int)
					break;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (!atoi(v.value._str.c_str()))
					break;
			}

			cursor = 0;
			do {
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %s %d MATCH *", key.c_str(), cursor);

				if (reply.values[0].arr_value.values.size() < 2) {
					return;
				}

				v = reply.values[0].arr_value.values[0].second;
				arr = reply.values[0].arr_value.values[1].second;
				if (arr.arr_value.values.size() < 2)
					break;

				for (size_t i = 0; i < arr.arr_value.values.size(); i += 2) {

					if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					std::string key = arr.arr_value.values[i].second.value._str;
					if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
						server->kvPlayers[idx][key] = (arr.arr_value.values[i + 1].second.value._str);
					}
					else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						server->kvPlayers[idx][key] = arr.arr_value.values[i + 1].second.value._int;
					}

					if (std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_team_fields.end()) {
						ret->captured_player_fields.push_back(arr.arr_value.values[i].second.value._str);
					}
				}
				s.str("");
			} while (cursor != 0);
			idx++;
		} while (true);
		s.str("");

		idx = 0;

		do {
			s.str("");
			s << entry_name << "custkeys_team_" << idx;
			key = s.str();

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", key.c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				break;

			v = reply.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				if (!v.value._int)
					break;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (!atoi(v.value._str.c_str()))
					break;
			}

			cursor = 0;
			do {
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %s %d MATCH *", key.c_str(), cursor);
				if (reply.values.size() < 2 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					break;

				v = reply.values[0];
				arr = reply.values[1];

				if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
					if (v.arr_value.values.size() <= 0) {
						break;
					}
					for (size_t i = 0; i < arr.arr_value.values.size(); i += 2) {

						if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
							continue;

						std::string key = arr.arr_value.values[i].second.value._str;
						if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvTeams[idx][key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvTeams[idx][key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if (std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_team_fields.end()) {
							ret->captured_team_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
				}
			} while (cursor != 0);
			idx++;
		} while (true);
	}
	void FetchServerRequestedKeys(TaskThreadData* thread_data, std::string server_key, Server* server, const sServerListReq* request, std::vector<CToken> token_list) {
		std::vector<std::string> lookup_keys;


		std::string command = "HMGET " + server_key + "custkeys";

		//these 2 are required for flags calculations
		lookup_keys.push_back("localip0");
		command += " localip0";

		lookup_keys.push_back("localport");
		command += " localport";

		std::vector<std::string>::const_iterator field_it = request->field_list.begin();
		while (field_it != request->field_list.end()) {
			const std::string field = *field_it;
			lookup_keys.push_back(field);			
			command += " " + field;
			field_it++;
		}

		std::vector<CToken>::iterator it = token_list.begin();
		while (it != token_list.end()) {
			CToken token = *it;
			if (token.getType() == EToken_Variable) {
				lookup_keys.push_back(token.getString());
				command += " " + token.getString();
			}

			it++;
		}

		Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, command.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 1)
			return;

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

			server->kvFields[lookup_keys.at(idx++)] = p.second.value._str;
		}
	}
	void FetchServerBasicInfo(TaskThreadData* thread_data, std::string server_key, Server *server) {
		std::vector<std::string> lookup_keys;

		lookup_keys.push_back("gameid");
		lookup_keys.push_back("id");
		lookup_keys.push_back("wan_port");
		lookup_keys.push_back("wan_ip");
		lookup_keys.push_back("icmp_address");
		lookup_keys.push_back("allow_unsolicited_udp");
		lookup_keys.push_back("country");

		std::string command = "HMGET " + server_key;

		server->key = server_key;

		server->kvFields["backend_id"] = server_key;

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
				idx++;
				continue;
			}
			switch (idx++) {
				case 0:
					server->game.gameid = atoi(p.second.value._str.c_str());
					break;
				case 1:
					server->id = atoi(p.second.value._str.c_str());
					break;
				case 2:
					server->wan_address.port = atoi(p.second.value._str.c_str());
					break;
				case 3:
					server->wan_address.ip = inet_addr(p.second.value._str.c_str());
					break;
				case 4:
					server->icmp_address.ip = inet_addr(p.second.value._str.c_str());
					break;
				case 5:
					server->allow_unsolicited_udp = atoi(p.second.value._str.c_str());
					break;
				case 6:
					server->kvFields["country"] = p.second.value._str;
					break;
			}
		}
	}
	void AppendServerEntry(TaskThreadData *thread_data, std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, const sServerListReq *req) {
		Redis::Response reply;
		int cursor = 0;

		std::string key;
		std::ostringstream s;		

		Redis::Value v, arr;

		std::vector<CToken> token_list = CToken::filterToTokenList(req->filter.c_str());

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);

		//skip deleted servers
		if (!include_deleted) {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s deleted", entry_name.c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
				return;
			}
			v = reply.values[0];
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) {
				return;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0) {
				return;
			}
		}


		Server *server = new MM::Server();

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s gameid", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();
		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			std::string gameid_reply = (v.value._str).c_str();
			int gameid = atoi(gameid_reply.c_str());
			if (req) {
				server->game = req->m_for_game;
			}
			else {				
				server->game = OS::GetGameByID(gameid, thread_data->mp_redis_connection);			
			}
			
			Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		}

		Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY %s -1 \"%s\"", server->game.gamename.c_str(), entry_name.c_str());


		FetchServerBasicInfo(thread_data, entry_name, server);

		if(all_keys) {
			FetchServerAllKeys(thread_data, entry_name, server, ret);
		} else {
			FetchServerRequestedKeys(thread_data, entry_name, server, req, token_list);
		}

		if(!req || filterMatches(token_list, server->kvFields)) {
			if (!req || (ret->list.size() < req->max_results || req->max_results == 0)) {
				ret->list.push_back(server);
			} else {
				delete server;
			}
		}
		else {
			delete server;
		}
		goto true_exit;

	error_cleanup:
			delete server;
	true_exit:
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
    }
}