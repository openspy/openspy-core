#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include <OS/HTTP.h>
#include <server/tasks/tasks.h>

#include <vector>

namespace FESL {
	EntitledGameFeature GetFeatureFromJson(json_t *json) {
		EntitledGameFeature result;
		json_t *item;
		item = json_object_get(json, "gameFeatureId");
		if(item && json_is_integer(item)) {
			result.GameFeatureId = json_integer_value(item);
		}

		item = json_object_get(json, "status");
		if(item && json_is_integer(item)) {
			result.Status = json_integer_value(item);
		}

		item = json_object_get(json, "message");
		if(item && json_is_string(item)) {
			result.Message = json_string_value(item);
		}

		item = json_object_get(json, "entitlementExpirationDate");
		if(item && json_is_integer(item)) {
			result.EntitlementExpirationDate = json_integer_value(item);
		} else {
			result.EntitlementExpirationDate = 0;
		}

		item = json_object_get(json, "entitlementExpirationDays");
		if(item && json_is_integer(item)) {
			result.EntitlementExpirationDays = json_integer_value(item);
		} else {
			result.EntitlementExpirationDays = -1;
		}
		return result;
	}
    bool Perform_GetEntitledGameFeatures(FESLRequest request, TaskThreadData *thread_data) {

		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.driverInfo.gameid));

		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/FESL/GameFeature/lookup";

		OS::HTTPClient client(url);

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskShared::WebErrorDetails error_details;

		std::vector<EntitledGameFeature> results;


		if (Handle_WebError(send_json, error_details)) {

		} else if(json_is_array(send_json)) {		
			json_t *value;
			size_t index;
			json_array_foreach(send_json, index, value) {
				EntitledGameFeature feature = GetFeatureFromJson(value);
				results.push_back(feature);
			}
		}

		json_decref(send_json);

		request.gameFeaturesCallback(error_details, results, request.peer);

		request.peer->DecRef();

        return true;
    }
}