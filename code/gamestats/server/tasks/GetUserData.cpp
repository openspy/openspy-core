#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
namespace GS {
	bool Perform_GetUserData(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.mp_peer->GetGame().gameid));

		json_object_set_new(send_json, "DataIndex", json_integer(request.data_index));
		json_object_set_new(send_json, "PersistType", json_integer(request.data_type));
		json_object_set_new(send_json, "modifiedSince", json_integer(request.modified_since));

		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Persist/Storage/GetData";

		OS::HTTPClient client(url);
		PersistBackendResponse resp_data;

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;
		if (json_is_array(send_json)) {
			success = true;
			if (json_array_size(send_json) > 0) {
				json_t *data_obj = json_array_get(send_json, 0);
				resp_data.game_instance_identifier = json_string_value(json_object_get(data_obj, "base64Data"));
			}
		}

		json_t *success_obj = json_object_get(send_json, "modified");
		if (success_obj) {
			resp_data.mod_time = (uint32_t)json_integer_value(success_obj);
		}
		callback_dispatch_later(success, resp_data, request.mp_peer, request.mp_extra, request.callback);

		json_decref(send_json);
		return false;
	}
}
