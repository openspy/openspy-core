#include "tasks.h"
namespace GP {
    bool Perform_GetBuddyStatus(GPBackendRedisRequest request, TaskThreadData *thread_data) {
		TaskShared::curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();
		json_t *profile_obj = json_object();
		json_object_set_new(profile_obj, "id", json_integer(request.peer->GetProfileID()));
		json_object_set_new(send_obj, "profileLookup", profile_obj);

		if(request.profile.id != 0) {
			json_t *target_obj = json_object();
			json_object_set_new(target_obj, "id", json_integer(request.profile.id));
			json_object_set_new(send_obj, "targetLookup", target_obj);
		}


		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		TaskShared::WebErrorDetails error_details;

		std::map<int, GPShared::GPStatus> results;

		if (curl) {
			struct curl_slist *chunk = NULL;
			GPReq_InitCurl(curl, json_dump, (void *)&recv_data, request, &chunk);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (TaskShared::Handle_WebError(json_data, error_details)) {
				}
				else if (json_data && json_is_array(json_data)) {
					int num_buddies = json_array_size(json_data);
					for(int i=0;i<num_buddies;i++) {

						json_t *array_item = json_array_get(json_data, i);

						//fetch profile data
						json_t *profile_json = json_object_get(array_item, "profile");
						if (profile_json) {
							profile = OS::LoadProfileFromJson(profile_json);
						}

						//fetch status data
						GPShared::GPStatus status;

						status.status = (GPShared::GPEnum)json_integer_value(json_object_get(array_item, "statusFlags"));
						status.status_str = json_string_value(json_object_get(array_item, "statusText"));
						status.location_str = json_string_value(json_object_get(array_item, "locationText"));
						status.quiet_flags = json_integer_value(json_object_get(array_item, "quietFlags"));
						status.address.ip = htonl(inet_addr(OS::strip_quotes(json_string_value(json_object_get(array_item, "ip"))).c_str()));
						status.address.port = (json_integer_value(json_object_get(array_item, "port")));
						results[profile.id] = status;
					}
				}
			}
			curl_slist_free_all(chunk);
			curl_easy_cleanup(curl);
		}

		
		//no callback yet..
		if(request.peer) {
			request.statusCallback(error_details, results, request.extra, request.peer);
		}

		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);

		if (request.peer)
			request.peer->DecRef();
		return true;
    }
}