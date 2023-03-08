#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {
	void AppendGroupEntries(TaskThreadData* thread_data, Redis::ArrayValue server_keys, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list);
	void LoadBasicGroupInfo(MM::Server* server, Redis::Value basic_keys_response);
	void LoadCustomGroupInfo(MM::Server* server, std::vector<std::string>& basic_lookup_keys, Redis::Value basic_keys_response);

    bool PerformGetGroups(MMQueryRequest request, TaskThreadData *thread_data) {
        GetGroups(thread_data, &request); //no need to free, because nothing appended to return value
		if(request.peer)
			request.peer->DecRef();
        return true;
    }
	ServerListQuery GetGroups(TaskThreadData *thread_data, const MMQueryRequest *request) {
		ServerListQuery ret;
		Redis::Response reply;
		Redis::Value v, arr;

		ret.requested_fields = request->req.field_list;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups);

		std::vector<CToken> token_list = CToken::filterToTokenList(request->req.filter.c_str());

		int cursor = 0;
		bool sent_servers = false;
		do {
			ServerListQuery streamed_ret;
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", request->req.m_for_game.gamename.c_str(), cursor);
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

			if (request->req.all_keys) {
				//XXX: append group entries ALL KEYS
			}
			else {
				AppendGroupEntries(thread_data, arr.arr_value, &streamed_ret, request, token_list);
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

	void AppendGroupEntries(TaskThreadData* thread_data, Redis::ArrayValue server_keys, ServerListQuery* query_response, const MMQueryRequest* request, std::vector<CToken> token_list) {

		if (server_keys.values.empty()) {
			return;
		}

		Redis::Response reply;

		std::ostringstream basic_lookup_str;

		std::vector<std::string> basic_lookup_keys;

		basic_lookup_keys.push_back("gameid");
		basic_lookup_keys.push_back("groupid");
		basic_lookup_keys.push_back("maxwaiting");
		basic_lookup_keys.push_back("hostname");
		basic_lookup_keys.push_back("password");
		basic_lookup_keys.push_back("numwaiting");
		basic_lookup_keys.push_back("numservers");

		std::vector<std::string>::iterator it = basic_lookup_keys.begin();
		while (it != basic_lookup_keys.end()) {
			basic_lookup_str << " " << *it;
			it++;
		}

		std::vector<std::string> lookup_keys;
		std::ostringstream lookup_str;


		std::vector<std::string>::const_iterator field_it = request->req.field_list.begin();
		while (field_it != request->req.field_list.end()) {
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

		std::ostringstream cmds;
		for (size_t i = 0; i < server_keys.values.size(); i += 2) {
			std::string server_key = server_keys.values[i].second.value._str;
			cmds << "HMGET " << server_key << basic_lookup_str.str() << "\r\n";
			cmds << "HMGET " << server_key << "custkeys" << lookup_str.str() << "\r\n";
		}

		reply = Redis::Command(thread_data->mp_redis_connection, REDIS_FLAG_NO_SUB_ARRAYS, cmds.str().c_str());
		for (size_t i = 0; i < server_keys.values.size(); i += 2) {
			Server server;

			Redis::Value basic_keys = reply.values.at(i);
			Redis::Value custom_keys = reply.values.at(i + 1);

			LoadBasicGroupInfo(&server, basic_keys);
			LoadCustomGroupInfo(&server, lookup_keys, custom_keys);

			if (!request || filterMatches(token_list, server.kvFields)) {
				if (!request || (query_response->list.size() < request->req.max_results || request->req.max_results == 0)) {
					Server* _server = new Server();
					*_server = server;
					query_response->list.push_back(_server);
				}
			}
		}
	}
	void LoadCustomGroupInfo(MM::Server* server, std::vector<std::string>& custom_lookup_keys, Redis::Value custom_keys_response) {
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
	void LoadBasicGroupInfo(MM::Server* server, Redis::Value basic_keys_response) {
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
					server->game.gameid = atoi(p.second.value._str.c_str());
				break;
				case 1:
					server->wan_address.ip = htonl(atoi((p.second.value._str).c_str())); //for V2
					server->kvFields["groupid"] = (p.second.value._str).c_str(); //for V1
				break;
				case 2:
					server->kvFields["maxwaiting"] = (p.second.value._str).c_str();
				break;
				case 3:
					server->kvFields["hostname"] = (p.second.value._str).c_str();
				break;
				case 4:
					server->kvFields["password"] = (p.second.value._str).c_str();
				break;
				case 5:
					server->kvFields["numwaiting"] = (p.second.value._str).c_str();
				break;
				case 6:
					server->kvFields["numservers"] = (p.second.value._str).c_str();
				break;

			}
		}
	}
}