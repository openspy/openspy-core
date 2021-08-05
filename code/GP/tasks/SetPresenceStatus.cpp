#include "tasks.h"
namespace GP {
	bool Perform_SetPresenceStatus(GPBackendRedisRequest request, TaskThreadData *thread_data) {
		TaskShared::curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object();

		
		json_object_set_new(send_obj, "ip", json_string(request.StatusInfo.address.ToString(true).c_str()));
		json_object_set_new(send_obj, "port", json_integer(request.StatusInfo.address.GetPort()));
		json_object_set_new(send_obj, "quietFlags", json_integer(request.StatusInfo.quiet_flags));
		json_object_set_new(send_obj, "statusFlags", json_integer(request.StatusInfo.status));
		json_object_set_new(send_obj, "statusText", json_string(request.StatusInfo.status_str.c_str()));
		json_object_set_new(send_obj, "locationText", json_string(request.StatusInfo.location_str.c_str()));

		if(request.profile.id != 0) {
			json_object_set_new(profile_obj, "id", json_integer(request.profile.id));
		} else {
			json_object_set_new(profile_obj, "id", json_integer(request.peer->GetProfileID()));
		}
		
		json_object_set_new(send_obj, "profileLookup", profile_obj);


		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		TaskShared::AuthData auth_data;
		bool success = false;

		if (curl) {
			struct curl_slist *chunk = NULL;
			GPReq_InitCurl(curl, json_dump, (void *)&recv_data, request, &chunk);

			res = curl_easy_perform(curl);

			curl_slist_free_all(chunk);
			curl_easy_cleanup(curl);
		}

		//no callback yet..

		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}