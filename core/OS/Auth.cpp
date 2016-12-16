#include <OS/OpenSpy.h>
#include <OS/Auth.h>
#include <curl/curl.h>
#include <jansson.h>
#include <jwt.h>
#include <string>


namespace OS {
	AuthTask *AuthTask::m_task_singleton = NULL;
	struct curl_data {
	    json_t *json_data;
	};
	/* callback for curl fetch */
	size_t AuthTask::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));

		//int jwt_decode(jwt_t **jwt, const char *token, const unsigned char *key,int key_len);
		jwt_decode(&jwt, (const char *)contents, NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);
		if(json) {
			data->json_data = json_loads(json, 0, NULL);
			free(json);
		}
		jwt_free(jwt);
	    return realsize;
	}
	void AuthTask::PerformAuth_NickEMail_GPHash(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "profilenick", json_string(request.nick.c_str()));
		json_object_set_new(send_obj, "email", json_string(request.email.c_str()));
		json_object_set_new(send_obj, "partnercode", json_integer(request.partnercode));
		json_object_set_new(send_obj, "hash_type", json_string("gp_nick_email"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", json_true());

		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		char *json_data = json_dumps(send_obj, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.session_key = NULL;
		auth_data.hash_proof = false;
		auth_data.response_code = (AuthResponseCode)-1;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSCoreAuth");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if(res == CURLE_OK) {
				json_t *success_obj = json_object_get(recv_data.json_data, "success");
				if(success_obj == json_true()) {
					json_t *profile_json = json_object_get(recv_data.json_data, "profile");
					if(profile_json) {
						profile = LoadProfileFromJson(profile_json);
						json_t *user_json = json_object_get(profile_json, "user");
						if(user_json) {
							user = LoadUserFromJson(user_json);
							success = true;
						}
					}
					json_t *server_response_json = json_object_get(recv_data.json_data, "server_response");
					if(server_response_json) {
						auth_data.hash_proof = json_string_value(server_response_json);
					}
					server_response_json = json_object_get(recv_data.json_data, "session_key");
					if(server_response_json) {
						auth_data.session_key = json_string_value(server_response_json);
					}

				} else {
				}
				json_t *reason_json = json_object_get(recv_data.json_data, "reason");
				if(reason_json) {
					auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
				}

				//typedef void (*AuthCallback)(bool success, User user, Profile profile, void *extra);
				request.callback(success, user, profile, auth_data, request.extra);
			}
			curl_easy_cleanup(curl);
		}
		jwt_free(jwt);
		free(json_data);
		json_decref(send_obj);
	}
	void AuthTask::TryAuthNickEmail_GPHash(std::string nick, std::string email, int partnercode, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra) {
		AuthRequest request;
		request.type = EAuthType_NickEmail_GPHash;
		request.email = email;
		request.nick = nick;
		request.partnercode = partnercode;
		request.server_challenge = server_chal;
		request.client_challenge = client_chal;
		request.client_response = client_response;
		request.extra = extra;
		request.callback = cb;

		AuthTask::getAuthTask()->AddRequest(request);
	}
	AuthTask::AuthTask() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(AuthTask::TaskThread, this, true);
	}
	AuthTask::~AuthTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	AuthTask *AuthTask::getAuthTask() {
		if(!AuthTask::m_task_singleton) {
			AuthTask::m_task_singleton = new AuthTask();
		}
		return AuthTask::m_task_singleton;
	}
	void *AuthTask::TaskThread(CThread *thread) {
		AuthTask *task = (AuthTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<AuthRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					AuthRequest task_params = *it;
					switch(task_params.type) {
						case EAuthType_NickEmail_GPHash:
							task->PerformAuth_NickEMail_GPHash(task_params);
						break;
					}
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			} else {
				OS::Sleep(TASK_SLEEP_TIME);
			}
		}
		return NULL;
	}
}