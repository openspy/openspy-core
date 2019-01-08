#include "tasks.h"
namespace GP {
		bool Perform_ToFromProfileAction(GPBackendRedisRequest request, TaskThreadData *thread_data) {
			TaskShared::curl_data recv_data;
			//build json object
			json_t *send_obj = json_object(), *to_obj = json_object(), *from_obj = json_object();


			json_object_set(send_obj, "sourceProfile", from_obj);
			json_object_set(send_obj, "targetProfile", to_obj);

			json_object_set_new(to_obj, "id", json_integer(request.ToFromData.to_profileid));
			json_object_set_new(from_obj, "id", json_integer(request.ToFromData.from_profileid));

			if (request.type == EGPRedisRequestType_DelBuddy) {
				//this might not be needed...
				json_object_set(send_obj, "silent", json_true());
			}


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
								json_t *user_json = json_object_get(profile_json, "user");
								if (user_json) {
									user = OS::LoadUserFromJson(user_json);
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