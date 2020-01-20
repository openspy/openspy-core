#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {
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
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
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
		
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}