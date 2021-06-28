#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {

	bool Perform_OperCheck(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		json_t* send_json = json_object();
		json_object_set_new(send_json, "profileid", json_integer(request.profile.id));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/GlobalOpers/lookup";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		TaskResponse response;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
            int operflags = 0;
			success = true;
			int num_items = json_array_size(send_json);
			for (int i = 0; i < num_items; i++) {
				json_t* item = json_array_get(send_json, i);

                json_t *operflags_item = json_object_get(item, "operflags");
                if(operflags_item != NULL) {
                    operflags |= json_integer_value(operflags_item);
                }

			}

            response.channel_summary.basic_mode_flags = operflags;

            if (request.callback)
                request.callback(response, request.peer);

		}

		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

}