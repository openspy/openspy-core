#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
namespace GS {
	bool Perform_SetUserData(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.mp_peer->GetGame().gameid));

		json_object_set_new(send_json, "DataIndex", json_integer(request.data_index));
		json_object_set_new(send_json, "PersistType", json_integer(request.data_type));
		json_object_set_new(send_json, "modifiedSince", json_integer(request.modified_since));

		std::string url = std::string(OS::g_webServicesURL);

		if (request.keyList.size() > 0) {
			url += "/v1/Persist/Storage/SetKVData";
			json_t *key_obj = json_object();
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it = request.kv_set_data.GetHead();
			std::ostringstream ss;
			while (it.first != it.second) {
				std::pair< std::string, std::string> item = *it.first;

				json_object_set_new(key_obj, item.first.c_str(), json_string(item.second.c_str()));
				it.first++;
			}

			json_object_set_new(send_json, "keyValueList", key_obj);
		}
		else {
			url += "/v1/Persist/Storage/SetData";
			json_object_set_new(send_json, "base64Data", json_string(request.game_instance_identifier.c_str()));
		}
		
		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

		OS::HTTPClient client(url);
		PersistBackendResponse resp_data;

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		json_t *success_obj = json_object_get(send_json, "modified");
		if (success_obj) {
			success = true;
			resp_data.mod_time = (uint32_t)json_integer_value(success_obj);
		}
		request.callback(success, resp_data, request.mp_peer, request.mp_extra);

		json_decref(send_json);
		return false;
	}
}
