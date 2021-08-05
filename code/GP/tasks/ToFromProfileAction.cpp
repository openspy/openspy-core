#include "tasks.h"
namespace GP {
		bool Perform_ToFromProfileAction(GPBackendRedisRequest request, TaskThreadData *thread_data) {
			TaskShared::curl_data recv_data;
			//build json object
			json_t *send_obj = json_object(), *to_obj = json_object(), *from_obj = json_object();


			json_object_set_new(send_obj, "sourceProfile", from_obj);
			json_object_set_new(send_obj, "targetProfile", to_obj);

			json_object_set_new(to_obj, "id", json_integer(request.ToFromData.to_profileid));
			json_object_set_new(from_obj, "id", json_integer(request.ToFromData.from_profileid));

			if (request.type == EGPRedisRequestType_DelBuddy) {
				//this might not be needed...
				json_object_set_new(send_obj, "silent", json_true());
			}

			if (request.auth_token.length()) {
				json_object_set_new(send_obj, "addReason", json_string(request.auth_token.c_str()));
			}


			char *json_dump = json_dumps(send_obj, 0);

			CURL *curl = curl_easy_init();
			CURLcode res;

			if (curl) {
				struct curl_slist *chunk = NULL;
				GPReq_InitCurl(curl, json_dump, (void *)&recv_data, request, &chunk);

				res = curl_easy_perform(curl);

				if (res == CURLE_OK) {
				}
				curl_slist_free_all(chunk);
				curl_easy_cleanup(curl);
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