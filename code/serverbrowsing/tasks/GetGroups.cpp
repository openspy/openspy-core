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
		Redis::Value v;
		Redis::ArrayValue arr;

		ret.requested_fields = req->field_list;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups);

		int cursor = 0;
		bool sent_servers = false;
		do {
			ServerListQuery streamed_ret;
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH %s:*:", cursor, req->m_for_game.gamename.c_str());
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
			arr = reply.values[0].arr_value.values[1].second.arr_value;

			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				cursor = atoi(v.value._str.c_str());
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				cursor = v.value._int;
			}

			if (cursor == 0) {
				streamed_ret.last_set = true;
			}
			for (size_t i = 0; i<arr.values.size(); i++) {
				if (request) {
					AppendGroupEntry(thread_data, arr.values[i].second.value._str.c_str(), &streamed_ret, req->all_keys, request);
				}
				else {
					AppendGroupEntry(thread_data, arr.values[i].second.value._str.c_str(), &ret, req->all_keys, request);
				}				
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

	void AppendGroupEntry(TaskThreadData *thread_data, const char *entry_name, ServerListQuery *ret, bool all_keys, const MMQueryRequest *request) {
		Redis::Response reply;
		Redis::Value v, arr;
		int cursor = 0;
		std::map<std::string, std::string> all_cust_keys; //used for filtering

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups);

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s gameid", entry_name);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if (request) {
			server->game = request->req.m_for_game;
		}
		else {
			server->game = OS::GetGameByID(atoi((v.value._str).c_str()), thread_data->mp_redis_connection);
		}
		
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_SBGroups); //change context back to SB db id

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s groupid", entry_name);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		server->wan_address.ip = htonl(atoi((v.value._str).c_str())); //for V2
		server->kvFields["groupid"] = (v.value._str).c_str(); //for V1
		
		FindAppend_ServKVFields(thread_data, server, entry_name, "maxwaiting");
		FindAppend_ServKVFields(thread_data, server, entry_name, "hostname");
		FindAppend_ServKVFields(thread_data, server, entry_name, "password");
		FindAppend_ServKVFields(thread_data, server, entry_name, "numwaiting");
		FindAppend_ServKVFields(thread_data, server, entry_name, "numservers");

		if(all_keys) {
	
			do {
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN %scustkeys %d match *", entry_name);
				if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
					goto error_cleanup;

				v = reply.values[0].arr_value.values[0].second;
				arr = reply.values[0].arr_value.values[1].second;
				if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
					if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						cursor = atoi(v.value._str.c_str());
					} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
						cursor = v.value._int;
					}

					if(arr.arr_value.values.size() <= 0) {
						break;
					}

					for(size_t i=0;i<arr.arr_value.values.size();i++) {
						std::string search_key = entry_name;
						search_key += "custkeys";
						FindAppend_ServKVFields(thread_data, server, search_key, arr.arr_value.values[i].second.value._str);
						if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), key) == ret->captured_basic_fields.end()) {
							ret->captured_basic_fields.push_back(arr.arr_value.values[i].second.value._str);
						}
					}
				}
			} while(cursor != 0);

		} else {

			while (it != ret->requested_fields.end()) {
				std::string field = *it;
				std::string entry = entry_name;
				entry += "custkeys";
				FindAppend_ServKVFields(thread_data, server, entry, field);
				it++;
			}
		}

		all_cust_keys = server->kvFields;
		if (!request || filterMatches(request->req.filter.c_str(), all_cust_keys)) {
			if (!request || (ret->list.size() < request->req.max_results || request->req.max_results == 0)) {
				ret->list.push_back(server);
			}
		}			
		return;

	error_cleanup:
		delete server;
    }
}