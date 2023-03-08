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

	ServerListQuery GetServers(TaskThreadData *thread_data, const MMQueryRequest *request) {
		ServerListQuery ret;

		Redis::Response reply;
		Redis::Value v, arr;

		ret.requested_fields = request->req.field_list;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		
		int cursor = 0;
		bool sent_servers = false;

		do {
			ServerListQuery streamed_ret;
			streamed_ret.requested_fields = ret.requested_fields;
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", request->req.m_for_game.gamename.c_str(), cursor);
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

			std::vector<std::string> server_keys;

			for (size_t i = 0; i < arr.arr_value.values.size(); i += 2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
				server_keys.push_back(server_key);
			}

			AppendServerEntries(thread_data, server_keys, &streamed_ret, &request->req);


			if (request && (!streamed_ret.list.empty() || streamed_ret.last_set)) {
				if (!sent_servers) {
					streamed_ret.first_set = true;
					sent_servers = true;
				}
				request->peer->OnRetrievedServers(*request, streamed_ret, request->extra);
			}

			FreeServerListQuery(&streamed_ret);
		} while(cursor != 0);

		error_cleanup:
			return ret;
	}

	void BuildServerListRequestData(const sServerListReq* req, std::vector<std::string>& basic_lookup_keys, std::vector<std::string>& lookup_keys, std::ostringstream& basic_lookup_str, std::ostringstream& lookup_str, std::vector<CToken> &out_token_list) {
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

		std::vector<std::string>::iterator it = basic_lookup_keys.begin();
		while (it != basic_lookup_keys.end()) {
			basic_lookup_str << " " << *it;
			it++;
		}

		if (req == NULL) {
			return;
		}

		std::vector<std::string>::const_iterator field_it = req->field_list.begin();
		while (field_it != req->field_list.end()) {
			const std::string field = *field_it;
			lookup_keys.push_back(field);
			lookup_str << " " << field;
			field_it++;
		}

		std::vector<CToken>::iterator itf = token_list.begin();
		while (itf != token_list.end()) {
			CToken token = *itf;
			if (token.getType() == EToken_Variable) {
				lookup_keys.push_back(token.getString());
				lookup_str << " " << token.getString();
			}
			itf++;
		}
	}

	void AppendServerEntries_AllKeys(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req) {
		std::vector<std::string> basic_lookup_keys;
		std::ostringstream basic_lookup_str;

		std::vector<std::string> lookup_keys;
		std::ostringstream lookup_str;

		std::vector<CToken> token_list;

		BuildServerListRequestData(req, basic_lookup_keys, lookup_keys, basic_lookup_str, lookup_str, token_list);

		std::ostringstream cmds;


		std::vector<std::string>::iterator it = server_keys.begin();
		while (it != server_keys.end()) {
			std::string server_key = *it;
			cmds << "HMGET " << server_key << basic_lookup_str.str() << "\r\n";
			cmds << "HGETALL " << server_key << "custkeys" << "\r\n";
			it++;
		}

		Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());

		for (size_t i = 0, c = 0; c < server_keys.size(); i += 2, c++) {
			Server server;

			Redis::Value basic_keys = reply.values.at(i);
			Redis::Value custom_keys = reply.values.at(i + 1);
			std::string server_key = server_keys.at(c);

			if (!LoadBasicServerInfo(&server, basic_keys)) {
				Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", req->m_for_game.gamename.c_str(), server_key.c_str());
				continue;
			}
			
			//skip deleted server
			if (!req->include_deleted && server.deleted) {
				continue;
			}
			LoadCustomServerInfo_AllKeys(&server, custom_keys);
			server.kvFields["backend_id"] = server_key;

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
	}

	void LoadCustomInfo_AllPlayerKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key) {
		int idx = 0;

		Redis::Response reply;
		Redis::Value v;

		do {
			std::ostringstream s;
			s << server_key << "custkeys_player_" << idx;

			std::string key = s.str();

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
			idx++;
		} while (true);

		if (idx == 0) {
			return;
		}

		//we got the max player key, now dump them
		std::ostringstream cmds;
		for (int i = 0; i < idx; i++) {
			cmds << "HGETALL " << server_key << "custkeys_player_" << i << "\r\n";
		}

		reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());

		for (int i = 0; i < idx; i++) {
			auto array = reply.values.at(i).arr_value.values;
			for (size_t c = 0; c < array.size(); c += 2) {
				std::string field_name = array.at(c).second.value._str;
				std::string field_value = array.at(c + 1).second.value._str;
				server->kvPlayers[i][field_name] = field_value;
			}
		}
	}

	void LoadCustomInfo_AllTeamKeys(TaskThreadData* thread_data, MM::Server* server, std::string server_key) {
		int idx = 0;

		Redis::Response reply;
		Redis::Value v;

		do {
			std::ostringstream s;
			s << server_key << "custkeys_team_" << idx;

			std::string key = s.str();

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
			idx++;
		} while (true);

		if (idx == 0) {
			return;
		}

		//we got the max player key, now dump them
		std::ostringstream cmds;
		for (int i = 0; i < idx; i++) {
			cmds << "HGETALL " << server_key << "custkeys_team_" << i << "\r\n";
		}

		reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());

		for (int i = 0; i < idx; i++) {
			auto array = reply.values.at(i).arr_value.values;
			for (size_t c = 0; c < array.size(); c += 2) {
				std::string field_name = array.at(c).second.value._str;
				std::string field_value = array.at(c + 1).second.value._str;
				server->kvTeams[i][field_name] = field_value;
			}
		}
	}

	void AppendServerEntries(TaskThreadData* thread_data, std::vector<std::string> server_keys, ServerListQuery* query_response, const sServerListReq* req) {
		Redis::Response reply;

		std::ostringstream basic_lookup_str;
		std::vector<std::string> basic_lookup_keys;

		std::vector<std::string> lookup_keys;
		std::ostringstream lookup_str;

		std::vector<CToken> token_list;

		if (server_keys.empty()) {
			return;
		}

		
		if (req->all_keys) {
			AppendServerEntries_AllKeys(thread_data, server_keys, query_response, req);
			return;
		}
		
		BuildServerListRequestData(req, basic_lookup_keys, lookup_keys, basic_lookup_str, lookup_str, token_list);

		bool has_customkeys = !lookup_keys.empty();


		std::ostringstream cmds;

		std::vector<std::string>::iterator it = server_keys.begin();
		while (it != server_keys.end()) {
			std::string server_key = *it;
			cmds << "HMGET " << server_key << basic_lookup_str.str() << "\r\n";
			if (has_customkeys) {
				cmds << "HMGET " << server_key << "custkeys" << lookup_str.str() << "\r\n";
			}			
			it++;
		}

		reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());

		it = server_keys.begin();
		int i = 0;
		while (it != server_keys.end()) {
			Server server;

			Redis::Value basic_keys = reply.values.at(i);
			
			std::string server_key = *it;

			if (!LoadBasicServerInfo(&server, basic_keys)) {
				//server is deleted / doesn't exist, remove from cache
				if (req) {
					Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s\"", req->m_for_game.gamename.c_str(), server_key.c_str());
				}	
			}
			else if(req->include_deleted || server.deleted == false) {
				if (has_customkeys) {
					Redis::Value custom_keys = reply.values.at(i + 1);
					LoadCustomServerInfo(&server, lookup_keys, custom_keys);
				}
				server.kvFields["backend_id"] = server_key;

				if (!req || filterMatches(token_list, server.kvFields)) {
					if (!req || (query_response->list.size() < req->max_results || req->max_results == 0)) {
						Server* _server = new Server();
						*_server = server;
						query_response->list.push_back(_server);
					}
				}
			}

			i += has_customkeys ? 2 : 1;
			it++;
		}
	}
	void LoadCustomServerInfo_AllKeys(MM::Server* server, Redis::Value custom_keys_response) {
		for (size_t i = 0; i < custom_keys_response.arr_value.values.size(); i += 2) {
			std::string field_name = custom_keys_response.arr_value.values.at(i).second.value._str;
			std::string field_value = custom_keys_response.arr_value.values.at(i + 1).second.value._str;
			server->kvFields[field_name] = field_value;
		}
	}

	void LoadCustomServerInfo(MM::Server* server, std::vector<std::string>& custom_lookup_keys, Redis::Value custom_keys_response) {
		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> >::iterator it = custom_keys_response.arr_value.values.begin();
		int idx = 0;
		while (it != custom_keys_response.arr_value.values.end()) {
			std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> p = *it;
			it++;

			if (p.first != Redis::REDIS_RESPONSE_TYPE_STRING) {
				idx++;
				continue;
			}

			std::string field_name = custom_lookup_keys.at(idx);
			std::string field_value = p.second.value._str;

			server->kvFields[field_name] = field_value;
			idx++;
		}
	}

	bool LoadBasicServerInfo(MM::Server* server, Redis::Value basic_keys_response) {
		std::vector<std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> >::iterator it = basic_keys_response.arr_value.values.begin();

		int idx = 0;
		while (it != basic_keys_response.arr_value.values.end()) {
			std::pair<Redis::REDIS_RESPONSE_TYPE, Redis::Value> p = *it;
			it++;

			if (p.first != Redis::REDIS_RESPONSE_TYPE_STRING) {
				idx++;
				continue;
			}
			switch (idx++) {
			case 0:
				if (p.second.type != Redis::REDIS_RESPONSE_TYPE_NULL) {
					if (atoi(p.second.value._str.c_str()) != 0) {
						//mark as deleted (we only remove non-existent from the cache)
						server->deleted = true;
					}
				}
				break;
			case 1:
				server->game.gameid = atoi(p.second.value._str.c_str());
				break;
			case 2:
				server->id = atoi(p.second.value._str.c_str());
				break;
			case 3:
				server->wan_address.port = atoi((p.second.value._str).c_str());
				break;
			case 4:
				server->wan_address.ip = inet_addr((p.second.value._str).c_str());
				break;
			case 5:
				server->icmp_address.ip = atoi((p.second.value._str).c_str());
				break;
			case 6:
				server->allow_unsolicited_udp = atoi((p.second.value._str).c_str());
				break;
			case 7:
				server->kvFields["country"] = p.second.value._str;
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