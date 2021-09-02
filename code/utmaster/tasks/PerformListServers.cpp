#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
	std::string GetHKey(TaskThreadData *thread_data, std::string server_key, std::string key) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s %s", server_key.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return "";

		v = reply.values.front();

		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING)
			return v.value._str;
		return "";
	}
	int GetHKeyInt(TaskThreadData *thread_data, std::string server_key, std::string key) {
		return atoi(GetHKey(thread_data, server_key, key).c_str());
	}
    ServerRecord LoadServerInfo(UTMasterRequest request, TaskThreadData *thread_data, std::string server_key) {
        ServerRecord result;
		
		result.id = GetHKeyInt(thread_data, server_key, "id");
		result.gameid = GetHKeyInt(thread_data, server_key, "gameid");
		result.m_address.port = GetHKeyInt(thread_data, server_key, "wan_port");
		result.m_address.ip = inet_addr(GetHKey(thread_data, server_key, "wan_ip").c_str());

		std::string cust_keys = server_key + "custkeys";
		printf("lookup: %s\n", cust_keys.c_str());

		result.hostname = GetHKey(thread_data, cust_keys, "hostname");
		result.level = GetHKey(thread_data, cust_keys, "mapname");
		result.game_group = GetHKey(thread_data, cust_keys, "game_group");
		result.num_players = GetHKeyInt(thread_data, cust_keys, "numplayers");
		result.max_players = GetHKeyInt(thread_data, cust_keys, "maxplayers");

		//XXX: iterate cust keys
		
        return result;
    }
    void GetServers(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, std::vector<ServerRecord> &results) {
		Redis::Response reply;
		Redis::Value v, arr;


		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		
		int cursor = 0;

		do {

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN %s %d", game_info.gamename.c_str(), cursor);
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

			for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
				std::string server_key = arr.arr_value.values[i].second.value._str;
                ServerRecord record = LoadServerInfo(request, thread_data, server_key);
                results.push_back(record);

			}
		} while(cursor != 0);

		error_cleanup:
			return;
    }
    bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data) {
        OS::GameData game_info = OS::GetGameByID(request.peer->GetGameId(), thread_data->mp_redis_connection);

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