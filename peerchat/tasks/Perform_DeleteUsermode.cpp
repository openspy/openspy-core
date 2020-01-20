#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {
	bool Perform_DeleteUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		json_t* send_json = json_object();
		json_object_set_new(send_json, "id", json_integer(request.usermodeRecord.usermodeid));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Delete(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		TaskResponse response;
		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (send_json) {
				json_t* success_obj = json_object_get(send_json, "success");
				if (success_obj) {
					success = success_obj == json_true();
				}
				if (!success) {
					response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
				}
			}
		}
		
		if (request.callback)
			request.callback(response, request.peer);

		if (request.peer)
			request.peer->DecRef();
		return true;

	}
}