#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	bool Perform_LookupGlobalUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {

		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelName", json_string("x"));

        json_t *user_summary = json_object();
        json_object_set_new(user_summary, "hostname", json_string(request.summary.address.ToString(true).c_str()));
        json_object_set_new(user_summary, "realname", json_string(request.summary.realname.c_str()));
        json_object_set_new(send_json, "userSummary", user_summary);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode/GetEffectiveUsermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskResponse response;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
            response.error_details.response_code = TaskShared::WebErrorCode_Success;
            response.usermode = GetUsermodeFromJson(send_json);
            
            if (request.callback) {
                request.callback(response, request.peer);
            }
		}



		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
    }
}