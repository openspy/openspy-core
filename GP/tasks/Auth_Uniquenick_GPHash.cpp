#include "tasks.h"
namespace GP {
	bool Perform_Auth_Uniquenick_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data) {
		TaskShared::curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object();

		json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if (request.profile.namespaceid != -1)
			json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));


		json_object_set(send_obj, "profile", profile_obj);

		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));


		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		TaskShared::AuthData auth_data;
		bool success = false;

		auth_data.response_code = TaskShared::LOGIN_RESPONSE_SERVER_ERROR;


		if (curl) {
			GPReq_InitCurl(curl, json_dump, (void *)&recv_data, request);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (json_data) {
					json_t *error_obj = json_object_get(json_data, "error");
					json_t *success_obj = json_object_get(json_data, "success");
					if (error_obj) {
						Handle_WebError(auth_data, error_obj);
					}
					else if (success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if (profile_json) {
							profile = OS::LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(json_data, "user");
							if (user_json) {
								user = OS::LoadUserFromJson(user_json);
								auth_data.response_code = TaskShared::LOGIN_RESPONSE_SUCCESS;
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "server_response");
						if (server_response_json) {
							auth_data.response_proof = json_string_value(server_response_json);
						}
						json_t *session_key_json = json_object_get(json_data, "session_key");
						if (session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}

					}
					json_decref(json_data);
				}
			}
			curl_easy_cleanup(curl);
		}
		request.authCallback(success, user, profile, auth_data, request.extra, request.peer);
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}