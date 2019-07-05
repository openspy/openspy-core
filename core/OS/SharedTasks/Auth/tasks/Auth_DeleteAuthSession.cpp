#include <OS/SharedTasks/tasks.h>
#include <OS/Net/NetPeer.h>
#include "../AuthTasks.h"
namespace TaskShared {
    bool PerformAuth_DeleteAuthSession(AuthRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "sessionKey", json_string(request.gamename.c_str()));
        
		
		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		TaskShared::AuthData auth_data;

		bool success = false;
		if (curl) {
			struct curl_slist *chunk = NULL;
			AuthReq_InitCurl(curl, json_dump, (void *)&recv_data, request, &chunk);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (Handle_WebError(json_data, auth_data.error_details)) {

				} else if (json_data) {
                    json_t *success_obj  = json_object_get(json_data, "success");
                    success = success_obj == json_true();
					json_decref(json_data);
				}
			}
			curl_slist_free_all(chunk);
			curl_easy_cleanup(curl);
		}
		request.callback(success, OS::User(), OS::Profile(), auth_data, request.extra, request.peer);
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);

		if (request.peer) {
			request.peer->DecRef();
		}
		return false;
    }
}