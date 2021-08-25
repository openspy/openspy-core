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
	void AppendServerEntry(TaskThreadData *thread_data, std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, const sServerListReq *req) {
		Redis::Response reply;
		int cursor = 0;
		int idx = 0;

		std::string key;
		std::ostringstream s;		

		Redis::Value v, arr;

		std::map<std::string, std::string> all_cust_keys; //used for filtering

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

		server->key = entry_name;

		server->kvFields["backend_id"] = entry_name;

		Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY %s -1 \"%s\"", server->game.gamename.c_str(), entry_name.c_str());


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s id", entry_name.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->id = atoi((v.value._str).c_str());

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s wan_port", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type== Redis::REDIS_RESPONSE_TYPE_STRING)
			server->wan_address.port = atoi((v.value._str).c_str());

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s wan_ip", entry_name.c_str());

		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->wan_address.ip = inet_addr((v.value._str).c_str());

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s icmp_address", entry_name.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->icmp_address.ip = inet_addr((v.value._str).c_str());

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s allow_unsolicited_udp", entry_name.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->allow_unsolicited_udp = atoi((v.value._str).c_str()) != 0;

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s country", entry_name.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			server->kvFields["country"] = v.value._str;

		if(all_keys) {
			do {
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %scustkeys %d MATCH *", entry_name.c_str(), cursor);
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
						if (arr.arr_value.values[i+1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvFields[key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if(arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvFields[key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), key) == ret->captured_basic_fields.end()) {
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
						goto error_cleanup;
					}

					v = reply.values[0].arr_value.values[0].second;
					arr = reply.values[0].arr_value.values[1].second;
					if (arr.arr_value.values.size() < 2)
						break;

					for (size_t i = 0; i<arr.arr_value.values.size(); i += 2) {

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
					if (reply.values.size() < 2|| reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
						break;

					v = reply.values[0];
					arr = reply.values[1];

					if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
						if (v.arr_value.values.size() <= 0) {
							break;
						}
						for (size_t i = 0; i<arr.arr_value.values.size(); i += 2) {

							if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
								continue;

							std::string key = arr.arr_value.values[i].second.value._str;
							if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
								server->kvTeams[idx][key] = (arr.arr_value.values[i + 1].second.value._str);
							}
							else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
								server->kvTeams[idx][key] = arr.arr_value.values[i + 1].second.value._int;
							}

							if(std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_team_fields.end()) {
								ret->captured_team_fields.push_back(arr.arr_value.values[i].second.value._str);
							}
						}
					}
				} while(cursor != 0);
				idx++;
			} while(true);

			idx = 0;
			all_cust_keys = server->kvFields;
		} else {
			do {
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %scustkeys %d MATCH *", entry_name.c_str(), cursor);
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					goto error_cleanup;

				v = reply.values[0].arr_value.values[0].second;
				arr = reply.values[0].arr_value.values[1].second;

				if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
					if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						cursor = atoi(v.value._str.c_str());
					}
					else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						cursor = v.arr_value.values[0].second.value._int;
					}

					for (size_t i = 0; i<arr.arr_value.values.size(); i += 2) {

						if (arr.arr_value.values[1].first != Redis::REDIS_RESPONSE_TYPE_STRING)
							continue;

						std::string key = arr.arr_value.values[i].second.value._str;
						if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
							server->kvFields[key] = (arr.arr_value.values[i + 1].second.value._str);
						}
						else if (arr.arr_value.values[i + 1].first == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
							server->kvFields[key] = arr.arr_value.values[i + 1].second.value._int;
						}

						if (std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), arr.arr_value.values[i].second.value._str) == ret->captured_basic_fields.end()) {
							ret->captured_basic_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
				}
			} while(cursor != 0);


			all_cust_keys = server->kvFields;
			if(req->all_keys == false) {
				server->kvFields.clear(); //remove all keys
				std::map<std::string, std::string>::iterator it = all_cust_keys.begin();
				while(it != all_cust_keys.end()) {
					std::pair<std::string, std::string> p = *it;

					//add only keys which were requested
					if(std::find(ret->requested_fields.begin(), ret->requested_fields.end(), p.first) != ret->requested_fields.end()) {
						server->kvFields[p.first] = p.second;
					}
					it++;
				}
			}
		}

		if(!req || filterMatches(req->filter.c_str(), all_cust_keys)) {
			if (!req || (ret->list.size() < req->max_results || req->max_results == 0)) {
				ret->list.push_back(server);
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