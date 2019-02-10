#include <OS/SharedTasks/tasks.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "../AuthTasks.h"
namespace TaskShared {
    bool PerformAuth_MakeAuthTicket(AuthRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object();

		json_object_set_new(send_obj, "id", json_integer(request.profile.id));

		json_object_set_new(send_obj, "profile", profile_obj);
		
		json_object_set_new(send_obj, "mode", json_string("make_auth_ticket"));

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

			AuthReq_InitCurl(curl, json_dump, (void *)&recv_data, request);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (Handle_WebError(json_data, auth_data.error_details)) {

				}
				else if (json_data) {
					json_t *error_obj = json_object_get(json_data, "error");
					json_t *success_obj = json_object_get(json_data, "success");
					 if (success_obj == json_true()) {
						success = true;
						json_t *session_key_json = json_object_get(json_data, "ticket");
						if (session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}

						session_key_json = json_object_get(json_data, "challenge");
						if (session_key_json) {
							auth_data.response_proof = json_string_value(session_key_json);
						}
					}
					json_decref(json_data);
				}
			}
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