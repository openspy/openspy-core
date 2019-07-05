#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
#include <algorithm>
namespace GS {
	bool Perform_GetUserKeyedData(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.mp_peer->GetGame().gameid));

		json_object_set_new(send_json, "DataIndex", json_integer(request.data_index));
		json_object_set_new(send_json, "PersistType", json_integer(request.data_type));
		json_object_set_new(send_json, "modifiedSince", json_integer(request.modified_since));

		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

        json_t *keys_array = json_array();
        std::vector<std::string>::iterator it = request.keyList.begin();
        int i = 0;
        while(it != request.keyList.end()) {
            std::string s = *it;
			json_array_insert_new(keys_array, i++, json_string(s.c_str()));
            it++;
        }
        json_object_set_new(send_json, "keys", keys_array);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Persist/Storage/GetKVData";

		OS::HTTPClient client(url);
		PersistBackendResponse resp_data;

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

        std::vector<std::string> found_keys;

        int modified = 0;
		bool success = false;
		if (json_is_array(send_json)) {
			success = true;
			int num_keys = json_array_size(send_json);
            std::ostringstream ss;
            for(int i=0;i<num_keys;i++) {
				std::string key, value;
                json_t *item = json_array_get(send_json, i);
                json_t *sub_item = json_object_get(item, "keyName");
                found_keys.push_back(json_string_value(sub_item));
                ss << "\\" << json_string_value(sub_item);
                sub_item = json_object_get(item, "keyValue");
                ss << "\\" << json_string_value(sub_item);
                sub_item = json_object_get(item, "modified");
                
                if(sub_item) {
                    int value = json_integer_value(sub_item);
                    if(value > modified) {
                        modified = value;
                    }
                }
            }

            resp_data.kv_data = ss.str();
		}

        resp_data.mod_time = modified;        
        

		json_t *success_obj = json_object_get(send_json, "modified");
		if (success_obj) {
			resp_data.mod_time = (uint32_t)json_integer_value(success_obj);
		}
		request.callback(success, resp_data, request.mp_peer, request.mp_extra);

		json_decref(send_json);
		request.mp_peer->DecRef();
		return false;
	}
}
