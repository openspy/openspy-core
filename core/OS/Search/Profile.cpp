#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <curl/curl.h>
#include <jansson.h>
#include <jwt/jwt.h>

namespace OS {
	ProfileSearchTask *ProfileSearchTask::m_task_singleton = NULL;
	struct curl_data {
	    json_t *json_data;
	};
	/* callback for curl fetch */
	size_t ProfileSearchTask::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));

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
	void ProfileSearchTask::PerformSearch(ProfileSearchRequest request) {
		curl_data recv_data;
		std::vector<OS::Profile> results;
		std::map<int, OS::User> users_map;
		//build json object
		json_t *send_obj = json_object();

		switch(request.type) {
			default:
			case EProfileSearch_Profiles:
				json_object_set_new(send_obj, "mode", json_string("profile_search"));	
			break;
			case EProfileSearch_Buddies:
				json_object_set_new(send_obj, "mode", json_string("buddies_search"));	
			break;
			case EProfileSearch_Blocks:
				json_object_set_new(send_obj, "mode", json_string("blocks_search"));	
			break;
			case EProfileSearch_Buddies_Reverse:
				json_object_set_new(send_obj, "mode", json_string("buddies_reverse_search"));	
			break;
		}
		


		if(request.profileid != 0) {
			json_object_set_new(send_obj, "profileid", json_integer(request.profileid));
		} else {

			//user parameters
			if(request.email.length())
				json_object_set_new(send_obj, "email", json_string(request.email.c_str()));

			
			json_object_set_new(send_obj, "partnercode", json_integer(request.partnercode));


			//profile parameters
			if(request.nick.length())
				json_object_set_new(send_obj, "profilenick", json_string(request.nick.c_str()));

			if(request.uniquenick.length())
				json_object_set_new(send_obj, "uniquenick", json_string(request.uniquenick.c_str()));

			if(request.firstname.length())
				json_object_set_new(send_obj, "firstname", json_string(request.firstname.c_str()));

			if(request.lastname.length())
				json_object_set_new(send_obj, "lastname", json_string(request.lastname.c_str()));

			if(request.icquin)
				json_object_set_new(send_obj, "icquin", json_integer(request.icquin));


			if(request.namespaceids.size()) {
				json_t *namespaceids_json = json_array();

				//json_array_append_new(v_array, json_real(v));
				std::vector<int>::iterator it = request.namespaceids.begin();
				while(it != request.namespaceids.end()) {
					int v = *it;
					json_array_append_new(namespaceids_json, json_integer(v));
					it++;
				}

				json_object_set_new(send_obj, "namespaceids", namespaceids_json);

			}
		}

		if(request.target_profileids.size()) {
			json_t *profileids_json = json_array();

			std::vector<int>::iterator it = request.target_profileids.begin();
			while(it != request.target_profileids.end()) {
				int v = *it;
				json_array_append_new(profileids_json, json_integer(v));
				it++;
			}

			json_object_set_new(send_obj, "target_profileids", profileids_json);
		}


		char *json_data = json_dumps(send_obj, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt); 
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchProfile");

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
					success = true;
					json_t *profiles_obj = json_object_get(recv_data.json_data, "profiles");
					if(profiles_obj) {
						int num_profiles = json_array_size(profiles_obj);
						for(int i=0;i<num_profiles;i++) {

							json_t *profile_obj = json_array_get(profiles_obj, i);
							OS::Profile profile = OS::LoadProfileFromJson(profile_obj);
							if(users_map.find(profile.userid) == users_map.end()) {
								json_t *user_obj = json_object_get(profile_obj, "user");
								users_map[profile.userid] = OS::LoadUserFromJson(user_obj);
							}
							results.push_back(profile);
						}
					}
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
		request.callback(success, results, users_map, request.extra);
	}

	void *ProfileSearchTask::TaskThread(CThread *thread) {
		ProfileSearchTask *task = (ProfileSearchTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<ProfileSearchRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					ProfileSearchRequest task_params = *it;
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
	ProfileSearchTask::ProfileSearchTask() : Task<ProfileSearchRequest>() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(ProfileSearchTask::TaskThread, this, true);

	}
	ProfileSearchTask::~ProfileSearchTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	ProfileSearchTask *ProfileSearchTask::getProfileTask() {
		if(!ProfileSearchTask::m_task_singleton) {
			ProfileSearchTask::m_task_singleton = new ProfileSearchTask();
		}
		return ProfileSearchTask::m_task_singleton;
	}
}