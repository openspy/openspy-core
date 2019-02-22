#include <OS/SharedTasks/tasks.h>
#include <OS/Net/NetPeer.h>
#include "../AuthTasks.h"
namespace TaskShared {
    bool PerformAuth_Email_Password(AuthRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object(), *user_obj = json_object();

		json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));

		json_object_set_new(send_obj, "profileLookup", profile_obj);

		json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		json_object_set_new(send_obj, "userLookup", user_obj);

		json_object_set_new(send_obj, "password", json_string(request.user.password.c_str()));
		
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

				} else if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						success = true;
						json_t *session_obj = json_object_get(json_data, "session");
						json_t *session_key_json = json_object_get(session_obj, "sessionKey");
						if (session_key_json) {
								auth_data.session_key = json_string_value(session_key_json);
						}

						session_key_json = json_object_get(session_obj, "expiresAt");
						if (session_key_json) {
								auth_data.expiresAt = json_integer_value(session_key_json);
						}

						session_key_json = json_object_get(session_obj, "expiresIn");
						if (session_key_json) {
								auth_data.expiresInSecs = json_integer_value(session_key_json);
						}

						session_key_json = json_object_get(json_data, "profile");
						if (session_key_json) {
							profile = OS::LoadProfileFromJson(session_key_json);
						}

						session_key_json = json_object_get(json_data, "user");
						if (session_key_json) {
							user = OS::LoadUserFromJson(session_key_json);
						}
					}
					json_decref(json_data);
				}
			}
			curl_easy_cleanup(curl);
		}
		request.callback(success, user, profile, auth_data, request.extra, request.peer);
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