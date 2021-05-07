#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	UsermodeRecord GetUsermodeFromJson(json_t* item) {
		UsermodeRecord record;

		json_t* subitem;

		record.isGlobal = true;

		subitem = json_object_get(item, "id");
		if (subitem != NULL) {
			record.usermodeid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "channelmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.chanmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "hostmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.hostmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "comment");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.comment = json_string_value(subitem);
		}

		subitem = json_object_get(item, "machineid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.machineid = json_string_value(subitem);
		}

		subitem = json_object_get(item, "profileid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "modeflags");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.modeflags = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "expiresAt");
		record.expires_at.tv_sec = 0;
		record.expires_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.expires_at.tv_sec = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "ircNick");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.nick = json_string_value(subitem);
		}

		subitem = json_object_get(item, "setByHost");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.address = OS::Address(json_string_value(subitem));
		}

		subitem = json_object_get(item, "setByPid");
		record.setByUserSummary.profileid = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "setAt");
		record.set_at.tv_sec = 0;
		record.set_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.set_at.tv_sec = json_integer_value(subitem);
		}

		
		return record;
	}
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

		bool success = false;

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
			success = true;
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
		}

		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

	bool Perform_ListUsermodes_Cached(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		int modeflags = 0;

		Redis::Response reply;
		Redis::Value v, arr;

		std::string keyname;

		int cursor = 0;

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);



		response.is_start = true;

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
					response.error_details.response_code = TaskShared::WebErrorCode_Success;

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
		return true;
	}
	void LoadUsermodeFromCache(TaskThreadData* thread_data, std::string cacheKey, UsermodeRecord &record) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		Redis::Response reply;
		Redis::Value v;


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s id", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.usermodeid = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s chanmask", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.chanmask = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s hostmask", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.hostmask = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s comment", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.comment = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s machineid", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.machineid = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s profileid", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.profileid = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s modeflags", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.modeflags = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s expiresAt", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.expires_at.tv_sec = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setAt", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.set_at.tv_sec = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setBy", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.setByUserSummary = UserSummary(OS::strip_quotes(v.value._str).c_str());
		}

		end_error:
		return;
	}
	bool UsermodeMatchesUser(UsermodeRecord usermode, UserSummary summary) {
		if(usermode.profileid != 0 && summary.profileid == usermode.profileid) {
			return true;
		}

		if(usermode.hostmask.length() > 0 && match2(usermode.hostmask.c_str(), summary.hostname.c_str())) {
			return true;
		}

		if(usermode.machineid.length() > 0 && match2(usermode.machineid.c_str(), summary.realname.c_str())) {
			return true;
		}
		return false;
	}
	int getEffectiveUsermode(TaskThreadData* thread_data, int channel_id, UserSummary summary, Peer *peer) {

		int modeflags = 0;

		Redis::Response reply;
		Redis::Value v, arr;

		UsermodeRecord record;

		std::string keyname;

		int cursor = 0;
		//scan channel usermodes
		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN channel_%d_usermodes %d match *", channel_id, cursor);
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
					if(i % 2 == 0) {
						keyname = "USERMODE_" + arr.arr_value.values[i].second.value._str;
						LoadUsermodeFromCache(thread_data, keyname, record);
						if(UsermodeMatchesUser(record, summary)) {
							modeflags |= record.modeflags;
						}
						
					}
						
				}
			}
		} while(cursor != 0);

		error_cleanup:
		return modeflags;
	}
}