#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/HTTP.h>
#include <server/tasks/tasks.h>

#include <vector>

namespace FESL {
	ObjectInventoryItem GetObjectFromJson(json_t *json) {
		ObjectInventoryItem result;

		json_t *item;
		item = json_object_get(json, "objectId");
		if(item && json_is_string(item)) {
			result.ObjectId = json_string_value(item);
		}

		item = json_object_get(json, "editionNo");
		if(item && json_is_integer(item)) {
			result.EditionNo = json_integer_value(item);
		} else {
			result.EditionNo = 0;
		}

		item = json_object_get(json, "useCount");
		if(item && json_is_integer(item)) {
			result.UseCount = json_integer_value(item);
		} else {
			result.UseCount = 0;
		}

		item = json_object_get(json, "entitleId");
		if(item && json_is_integer(item)) {
			result.EntitleId = json_integer_value(item);
		} else {
			result.EntitleId = 0;
		}

		item = json_object_get(json, "dateEntitled");
		if(item && json_is_integer(item)) {
			result.DateEntitled = json_integer_value(item);
		} else {
			result.DateEntitled = 0;
		}

		return result;
	}
    bool Perform_GetObjectInventory(FESLRequest request, TaskThreadData *thread_data) {

		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.driverInfo.gameid));

		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

        json_t *object_ids = json_array();
        std::vector<std::string>::iterator it = request.objectIds.begin();
        while(it != request.objectIds.end()) {
            std::string s = *it;
            json_array_append_new(object_ids, json_string(s.c_str()));
            it++;
        }


        json_object_set_new(send_json, "objectIds", object_ids);
        json_object_set_new(send_json, "domainId", json_string(request.driverInfo.domainPartition.c_str()));
        json_object_set_new(send_json, "subdomainId", json_string(request.driverInfo.subDomain.c_str()));        
        json_object_set_new(send_json, "partitionKey", json_string(request.driverInfo.messagingHostname.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/FESL/ObjectInventory/lookup";

		OS::HTTPClient client(url);

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskShared::WebErrorDetails error_details;

		std::vector<ObjectInventoryItem> results;

		if (Handle_WebError(send_json, error_details)) {

		} else if(json_is_array(send_json)) {		
			json_t *value;
			size_t index;
			json_array_foreach(send_json, index, value) {
				ObjectInventoryItem feature = GetObjectFromJson(value);
				results.push_back(feature);
			}
		}

		request.objectInventoryItemsCallback(error_details, results, request.peer, request.extra);

		json_decref(send_json);

		request.peer->DecRef();

        return true;
    }
}