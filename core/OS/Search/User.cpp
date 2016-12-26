#include <OS/OpenSpy.h>
#include <OS/Search/User.h>

#include <curl/curl.h>
#include <jansson.h>
#include <jwt/jwt.h>

namespace OS {
	UserSearchTask *UserSearchTask::m_task_singleton = NULL;
	struct curl_data {
	    json_t *json_data;
	};
	/* callback for curl fetch */
	size_t UserSearchTask::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_USERMGR_KEY, strlen(OPENSPY_USERMGR_KEY));

		jwt_decode(&jwt, (const char *)contents, NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		if(json) {
			data->json_data = json_loads(json, 0, NULL);
			free(json);
		} else {
			data->json_data = NULL;
		}
		jwt_free(jwt);
	    return realsize;
	}
	void UserSearchTask::PerformSearch(UserSearchRequest request) {
		curl_data recv_data;
		std::vector<OS::User> results;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "mode", json_string("get_user"));	

		if(request.userid != 0) {
			json_object_set_new(send_obj, "userid", json_integer(request.userid));
		}

		//user parameters
		if(request.email.length())
			json_object_set_new(send_obj, "email", json_string(request.email.c_str()));

		
		json_object_set_new(send_obj, "partnercode", json_integer(request.partnercode));


		char *json_data = json_dumps(send_obj, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt); 
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_USERMGR_KEY, strlen(OPENSPY_USERMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_USERMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchUser");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			if(res == CURLE_OK) {
				if(recv_data.json_data) {
					json_t *user_obj = json_object_get(recv_data.json_data, "user");
					if(user_obj) {
						OS::User user = OS::LoadUserFromJson(user_obj);
						results.push_back(user);
					}
					success = true;
				} else {
					success = false;
				}
			}
		}
		
		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}

		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
		request.callback(success, results, request.extra);
	}

	void *UserSearchTask::TaskThread(CThread *thread) {
		UserSearchTask *task = (UserSearchTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<UserSearchRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					UserSearchRequest task_params = *it;
					PerformSearch(task_params);
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(TASK_SLEEP_TIME);
		}
		return NULL;
	}
	UserSearchTask::UserSearchTask() : Task<UserSearchRequest>() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(UserSearchTask::TaskThread, this, true);

	}
	UserSearchTask::~UserSearchTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	UserSearchTask *UserSearchTask::getUserTask() {
		if(!UserSearchTask::m_task_singleton) {
			UserSearchTask::m_task_singleton = new UserSearchTask();
		}
		return UserSearchTask::m_task_singleton;
	}
}