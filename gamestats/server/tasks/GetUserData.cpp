#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
namespace GS {
	bool Perform_GetUserData(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(request.profileid));
		json_object_set_new(send_json, "mode", json_string("get_persist_data"));


		json_object_set_new(send_json, "data_index", json_integer(request.data_index));
		json_object_set_new(send_json, "data_type", json_integer(request.data_type));
		json_object_set_new(send_json, "game_id", json_integer(request.mp_peer->GetGame().gameid));
		json_object_set_new(send_json, "modified_since", json_integer(request.modified_since));

		if (request.keyList.size() > 0) {
			std::vector<std::string>::iterator it = request.keyList.begin();
			json_t *keylist_array = json_array();
			while (it != request.keyList.end()) {
				json_array_append(keylist_array, json_string((*it).c_str()));
				it++;
			}
			json_object_set(send_json, "keyList", keylist_array);
		}

		OS::HTTPClient client("");

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		json_t *data_obj = json_object_get(send_json, "data");


		PersistBackendResponse resp_data;

		if (data_obj) {
			resp_data.game_instance_identifier = json_string_value(data_obj);
		}

		data_obj = json_object_get(send_json, "keyList");

		const char *key;
		json_t *value;

		if (data_obj) {
			std::ostringstream ss;
			json_object_foreach(data_obj, key, value) {
				ss << "\\" << key << "\\" << json_string_value(value);
			}
			resp_data.kv_data = ss.str();
		}

		success_obj = json_object_get(send_json, "modified");
		if (success_obj) {
			resp_data.mod_time = (uint32_t)json_integer_value(success_obj);
		}
		else {
			resp_data.mod_time = 0;
		}

		request.callback(success, resp_data, request.mp_peer, request.mp_extra);

		json_decref(send_json);
		return false;
	}
}
