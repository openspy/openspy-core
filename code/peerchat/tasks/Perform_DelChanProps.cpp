#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {

	bool Perform_DelChanprops(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelmask", json_string(request.chanpropsRecord.channel_mask.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Chanprops";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Delete(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		TaskResponse response;

		TaskShared::Handle_WebError(send_json, response.error_details);
        if (request.callback)
                request.callback(response, request.peer);

		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

}