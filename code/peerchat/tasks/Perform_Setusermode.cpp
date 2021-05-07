#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {
	const char *mp_pk_temporary_usermode_id = "USERMODEID";
	int GetTemporaryUsermodeId(TaskThreadData *thread_data) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_temporary_usermode_id);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return -ret;
	}
	json_t* UsermodeRecordToJson(UsermodeRecord record) {
		json_t* object = json_object();
		if(record.chanmask.length() > 0)
			json_object_set(object, "channelmask", json_string(record.chanmask.c_str()));
		if (record.hostmask.length() > 0)
			json_object_set(object, "hostmask", json_string(record.hostmask.c_str()));
		if (record.comment.length() > 0)
			json_object_set(object, "comment", json_string(record.comment.c_str()));
		if (record.machineid.length() > 0)
			json_object_set(object, "machineid", json_string(record.machineid.c_str()));

		if (record.profileid != 0)
			json_object_set(object, "profileid", json_integer(record.profileid));

		json_object_set(object, "modeflags", json_integer(record.modeflags));

		json_object_set(object, "expiresIn", json_integer(record.expires_at.tv_sec)); //expires in seconds

		json_object_set(object, "ircNick", json_string(record.setByUserSummary.nick.c_str()));

		json_object_set(object, "setByHost", json_string(record.setByUserSummary.address.ToString().c_str()));
		json_object_set(object, "setByPid", json_integer(record.setByUserSummary.profileid));
		return object;
	}
	bool Perform_SetGlobalUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) { 
		TaskResponse response;
        
		request.usermodeRecord.setByUserSummary = request.peer->GetUserDetails();
		json_t* send_json = UsermodeRecordToJson(request.usermodeRecord);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {
			UsermodeRecord record = GetUsermodeFromJson(send_json);
			response.usermode = record;
		}

		if(send_json)
			json_decref(send_json);
		
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
	void WriteUsermodeToCache(UsermodeRecord usermode, TaskThreadData* thread_data) { 
		

		std::ostringstream s;
		s << "USERMODE_" << usermode.usermodeid;
		std::string key_name = s.str();

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", key_name.c_str(), usermode.usermodeid);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s chanmask \"%s\"", key_name.c_str(), usermode.chanmask.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s hostmask \"%s\"", key_name.c_str(), usermode.hostmask.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s comment \"%s\"", key_name.c_str(), usermode.comment.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s machineid \"%s\"", key_name.c_str(), usermode.machineid.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s profileid %d", key_name.c_str(), usermode.profileid);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s modeflags %d", key_name.c_str(), usermode.modeflags);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s isGlobal %d", key_name.c_str(), usermode.usermodeid > 0);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setBy \"%s\"", key_name.c_str(), usermode.setByUserSummary.ToString(true).c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setAt %d", key_name.c_str(), usermode.set_at.tv_sec);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s expiresAt %d", key_name.c_str(), usermode.expires_at.tv_sec);
	}
	bool Perform_SetTemporaryUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {

		request.usermodeRecord.usermodeid = GetTemporaryUsermodeId(thread_data);
		WriteUsermodeToCache(request.usermodeRecord, thread_data);
		return true;
	}
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		if(request.usermodeRecord.isGlobal == 0) {
			return Perform_SetTemporaryUsermode(request, thread_data);
		} else {
			return Perform_SetGlobalUsermode(request, thread_data);
		}

	}
}