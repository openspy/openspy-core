#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {
    bool PerformGetGroups(MMQueryRequest request, TaskThreadData *thread_data) {
        GetGroups(thread_data, &request.req, &request); //no need to free, because nothing appended to return value
		if(request.peer)
			request.peer->DecRef();
        return true;
    }
	ServerListQuery GetGroups(TaskThreadData *thread_data, const sServerListReq *req, const MMQueryRequest *request) {
		ServerListQuery ret;
		Redis::Response reply;
		Redis::Value v, arr;

		ret.requested_fields = req->field_list;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups);

		std::vector<CToken> token_list = CToken::filterToTokenList(request->req.filter.c_str());

		int cursor = 0;
		bool sent_servers = false;
		do {
			ServerListQuery streamed_ret;
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", req->m_for_game.gamename.c_str(), cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

			streamed_ret.last_set = false;
			if (cursor == 0) {
				streamed_ret.first_set = true;
			}
			else {
				streamed_ret.first_set = false;
			}
			if (reply.values[0].arr_value.values.size() < 2) {
				goto error_cleanup;
			}
			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				cursor = atoi(v.value._str.c_str());
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				cursor = v.value._int;
			}

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}

			for (size_t i = 0; i < arr.arr_value.values.size(); i += 2) {
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

				AppendGroupEntry(thread_data, server_key.c_str(), &streamed_ret, req->all_keys, request, token_list);
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

		error_cleanup:
			return ret;
	}

	void FetchGroupBasicInfo(TaskThreadData* thread_data, std::string server_key, Server* server) {
		std::vector<std::string> lookup_keys;

		lookup_keys.push_back("gameid");
		lookup_keys.push_back("groupid");
		lookup_keys.push_back("maxwaiting");
		lookup_keys.push_back("hostname");
		lookup_keys.push_back("password");
		lookup_keys.push_back("numwaiting");
		lookup_keys.push_back("numservers");

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
			switch (idx) {
			case 0:
				server->game.gameid = atoi(p.second.value._str.c_str());
				break;
			case 1:
				server->wan_address.ip = htonl(atoi((p.second.value._str).c_str())); //for V2
				server->kvFields["groupid"] = p.second.value._str; //for V1
				break;
			default:
				if (p.second.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					server->kvFields[lookup_keys.at(idx)] = p.second.value._str;
				}
				else if (p.second.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					server->kvFields[lookup_keys.at(idx)] = p.second.value._int;
				}				
				break;
			}
			idx++;
		}
	}

	void FetchGroupAllKeys(TaskThreadData* thread_data, std::string server_key, Server* server, ServerListQuery* ret) {
		Redis::Response reply;
		Redis::Value v, arr;
		int cursor = 0;
		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %scustkeys %d MATCH *", server_key.c_str(), cursor);
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
	error_cleanup:
		return;
	}

	void FetchGroupRequestedKeys(TaskThreadData* thread_data, std::string server_key, Server* server, const sServerListReq *request, std::vector<CToken> token_list) {
		std::vector<std::string> lookup_keys;


		std::string command = "HMGET " + server_key + "custkeys";

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

			if (p.second.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				server->kvFields[lookup_keys.at(idx)] = p.second.value._str;
			}
			else if (p.second.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				server->kvFields[lookup_keys.at(idx)] = p.second.value._int;
			}
			idx++;
		}
	}

	void AppendGroupEntry(TaskThreadData *thread_data, const char *entry_name, ServerListQuery *ret, bool all_keys, const MMQueryRequest *request, std::vector<CToken> token_list) {

		Redis::Response reply;
		Redis::Value v, arr;
		int cursor = 0;

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups);

		FetchGroupBasicInfo(thread_data, entry_name, server);

		if (all_keys) {
			FetchGroupAllKeys(thread_data, entry_name, server, ret);
		}
		else {
			FetchGroupRequestedKeys(thread_data, entry_name, server, &request->req, token_list);
		}


		if (!request || filterMatches(token_list, server->kvFields)) {
			if (!request || (ret->list.size() < request->req.max_results || request->req.max_results == 0)) {
				ret->list.push_back(server);
			}
		}			
		return;

	error_cleanup:
		delete server;
    }
}