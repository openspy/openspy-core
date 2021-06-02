#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {

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


	
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		return Perform_SetGlobalUsermode(request, thread_data);
	}
}