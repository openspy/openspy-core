#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {

	bool Perform_ListUsermodes(PeerchatBackendRequest request, TaskThreadData* thread_data) {

		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelmask", json_string(request.usermodeRecord.chanmask.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode/lookup";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
			int num_items = json_array_size(send_json);
			for (int i = 0; i < num_items; i++) {
				json_t* item = json_array_get(send_json, i);

				response.is_start = i == 0;
				response.is_end = i == num_items - 1;

				UsermodeRecord record = GetUsermodeFromJson(item);
				response.usermode = record;

				if (request.callback)
					request.callback(response, request.peer);
			}
			if(num_items == 0) {
				response.is_start = true;
				response.is_end = true;

				if (request.callback)
					request.callback(response, request.peer);
			}
		}

		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

	bool Perform_ListUsermodes_Cached(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		Redis::Response reply;
		Redis::Value v, arr;

		std::string keyname;

		int cursor = 0;

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		response.is_start = true;
		response.is_end = false;

		response.error_details.response_code = TaskShared::WebErrorCode_Success;

		//scan channel usermodes
		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH USERMODE_*", cursor);
			if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
				goto end_error;

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
					LoadUsermodeFromCache(thread_data, arr.arr_value.values[i].second.value._str, response.usermode);
					

					if (request.callback)
						request.callback(response, request.peer);

					response.is_start = false;
				}
			}
		} while(cursor != 0);

		response.is_end = true;
		response.usermode.usermodeid = 0;
		if (request.callback)
			request.callback(response, request.peer);

		end_error:

		if (request.peer) {
			request.peer->DecRef();
		}

		return true;
	}

}