#include "tasks.h"
namespace GP {
	bool Perform_Auth_NickEmail_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data) {
		TaskShared::curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if (request.profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));

		if (request.user.email.length())
			json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));
		json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));



		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);


		char *json_data_str = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		bool success = false;
		TaskShared::AuthData auth_data;

		if (curl) {
			GPReq_InitCurl(curl, json_data_str, (void *)&recv_data, request);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);

				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (TaskShared::Handle_WebError(json_data, auth_data.error_details)) {
					}
					else if (success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if (profile_json) {
							profile = OS::LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(json_data, "user");
							if (user_json) {
								user = OS::LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "server_response");
						if (server_response_json) {
							auth_data.response_proof = json_string_value(server_response_json);
						}
						server_response_json = json_object_get(json_data, "session_key");
						if (server_response_json) {
							auth_data.session_key = json_string_value(server_response_json);
						}

					}
					json_decref(json_data);
				}
			}
			curl_easy_cleanup(curl);
			request.authCallback(success, user, profile, auth_data, request.extra, request.peer);
			if (json_data_str) {
				free((void *)json_data_str);
			}
			json_decref(send_obj);
		}

		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}