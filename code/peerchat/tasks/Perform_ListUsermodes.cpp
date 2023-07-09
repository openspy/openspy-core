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
		redisReply *reply;

		std::string keyname;

		int cursor = 0;

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
		freeReplyObject(reply);

		response.is_start = true;
		response.is_end = false;

		response.error_details.response_code = TaskShared::WebErrorCode_Success;

		//scan channel usermodes
		do {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH USERMODE_*", cursor);
			if (reply == NULL)
				goto end_error;


			if(thread_data->mp_redis_connection->err || reply->elements < 2) {
				freeReplyObject(reply);
				goto end_error;
			}

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			for(size_t i=0;i<reply->element[1]->elements;i++) {
				LoadUsermodeFromCache(thread_data, reply->element[1]->element[i]->str, response.usermode);
				

				if (request.callback)
					request.callback(response, request.peer);

				response.is_start = false;
			}
			freeReplyObject(reply);
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