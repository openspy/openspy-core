#include "tasks.h"
namespace GP {
	bool Perform_Auth_PreAuth_Token_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data) {
		TaskShared::curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		if (request.auth_token.length())
			json_object_set_new(send_obj, "auth_token", json_string(request.auth_token.c_str()));

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

					}
					json_decref(json_data);
				}
			}
			curl_slist_free_all(chunk);
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