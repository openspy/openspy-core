#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <curl/curl.h>
#include <jansson.h>

#include <ctype.h>

namespace OS {
	OS::TaskPool<ProfileSearchTask, ProfileSearchRequest> *m_profile_search_task_pool = NULL;
	struct curl_data {
		std::string buffer;
	};
	/* callback for curl fetch */
	size_t ProfileSearchTask::curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
		if (!contents) {
			return 0;
		}
		size_t realsize = size * nmemb;                             /* calculate buffer size */
		curl_data *data = (curl_data *)userp;
		data->buffer += (const char *)contents;
		return realsize;
	}
	void ProfileSearchTask::PerformSearch(ProfileSearchRequest request) {
		curl_data recv_data;
		recv_data.buffer = "";
		std::vector<OS::Profile> results;
		std::map<int, OS::User> users_map;
		//build json object
		json_t *send_obj = json_object();

		switch (request.type) {
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
		case EProfileSearch_CreateProfile:
			json_object_set_new(send_obj, "mode", json_string("create_profile"));
			break;
		case EProfileSearch_DeleteProfile:
			json_object_set_new(send_obj, "mode", json_string("delete_profile"));
			break;
		case EProfileSearch_UpdateProfile:
			json_object_set_new(send_obj, "mode", json_string("update_profile"));
			break;
		}



		/*if (request.profile_search_details.id != 0 && request.type != EProfileSearch_UpdateProfile) {
			json_object_set_new(send_obj, "profileid", json_integer(request.profile_search_details.id));
		}
		else */ {

			json_t *profile_obj = json_object();
			json_t *user_obj = json_object();

			if (request.profile_search_details.id != 0)
				json_object_set_new(profile_obj, "id", json_integer(request.profile_search_details.id));

			//user parameters
			if (request.user_search_details.email.length()) {
				json_object_set_new(profile_obj, "email", json_string(request.user_search_details.email.c_str()));
				json_object_set_new(user_obj, "email", json_string(request.user_search_details.email.c_str()));
			}			

			if (request.user_search_details.id != 0) {
				//json_object_set_new(profile_obj, "userid", json_integer(request.user_search_details.id));
				json_object_set_new(user_obj, "id", json_integer(request.user_search_details.id));
			}


			json_object_set_new(user_obj, "partnercode", json_integer(request.user_search_details.partnercode));


			//profile parameters
			if (request.profile_search_details.nick.length())
				json_object_set_new(profile_obj, "nick", json_string(request.profile_search_details.nick.c_str()));

			if (request.profile_search_details.uniquenick.length())
				json_object_set_new(profile_obj, "uniquenick", json_string(request.profile_search_details.uniquenick.c_str()));

			if (request.profile_search_details.firstname.length())
				json_object_set_new(profile_obj, "firstname", json_string(request.profile_search_details.firstname.c_str()));

			if (request.profile_search_details.lastname.length())
				json_object_set_new(profile_obj, "lastname", json_string(request.profile_search_details.lastname.c_str()));

			if (request.profile_search_details.icquin)
				json_object_set_new(profile_obj, "icquin", json_integer(request.profile_search_details.icquin));

			if (request.profile_search_details.zipcode)
				json_object_set_new(profile_obj, "zipcode", json_integer(request.profile_search_details.zipcode));


			if(request.profile_search_details.sex != -1)
				json_object_set_new(profile_obj, "sex", json_integer(request.profile_search_details.sex));

			if(request.profile_search_details.pic != 0)
				json_object_set_new(profile_obj, "pic", json_integer(request.profile_search_details.pic));
			if (request.profile_search_details.ooc != 0)
				json_object_set_new(profile_obj, "ooc", json_integer(request.profile_search_details.ooc));
			if (request.profile_search_details.ind!= 0)
				json_object_set_new(profile_obj, "ind", json_integer(request.profile_search_details.ind));
			if (request.profile_search_details.mar!= 0)
				json_object_set_new(profile_obj, "mar", json_integer(request.profile_search_details.mar));
			if (request.profile_search_details.chc != 0)
				json_object_set_new(profile_obj, "chc", json_integer(request.profile_search_details.chc));
			if (request.profile_search_details.i1 != 0)
				json_object_set_new(profile_obj, "i1", json_integer(request.profile_search_details.i1));

			if (request.profile_search_details.birthday.GetYear() != 0)
				json_object_set_new(profile_obj, "birthday", request.profile_search_details.birthday.GetJson());

			if(request.profile_search_details.lon)
				json_object_set_new(profile_obj, "lon", json_real(request.profile_search_details.lon));
			if(request.profile_search_details.lat)
				json_object_set_new(profile_obj, "lat", json_real(request.profile_search_details.lat));


			if (request.namespaceids.size()) {
				json_t *namespaceids_json = json_array();

				//json_array_append_new(v_array, json_real(v));
				std::vector<int>::iterator it = request.namespaceids.begin();
				while (it != request.namespaceids.end()) {
					int v = *it;
					json_array_append_new(namespaceids_json, json_integer(v));
					it++;
				}

				json_object_set_new(profile_obj, "namespaceids", namespaceids_json);

			}
			json_object_set(send_obj, "profile", profile_obj);
			json_object_set(send_obj, "user", user_obj);
		}



		if (request.target_profileids.size()) {
			json_t *profileids_json = json_array();

			std::vector<int>::iterator it = request.target_profileids.begin();
			while (it != request.target_profileids.end()) {
				int v = *it;
				json_array_append_new(profileids_json, json_integer(v));
				it++;
			}

			json_object_set_new(send_obj, "target_profileids", profileids_json);
		}


		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		EProfileResponseType error = EProfileResponseType_GenericError;
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchProfile");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);
			if (res == CURLE_OK) {
				json_t *json_data = NULL;
				
				json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);

				if (json_data) {
					error = EProfileResponseType_Success;
					json_t *profiles_obj = json_object_get(json_data, "profiles");
					if (profiles_obj) {
						int num_profiles = json_array_size(profiles_obj);
						for (int i = 0; i < num_profiles; i++) {
							json_t *profile_obj = json_array_get(profiles_obj, i);
							OS::Profile profile = OS::LoadProfileFromJson(profile_obj);
							if (users_map.find(profile.userid) == users_map.end()) {
								json_t *user_obj = json_object_get(profile_obj, "user");
								users_map[profile.userid] = OS::LoadUserFromJson(user_obj);
							}
							results.push_back(profile);
						}
					}
					else {
						//check for single profile
						profiles_obj = json_object_get(json_data, "profile");
						if (profiles_obj) {
							OS::Profile profile = OS::LoadProfileFromJson(profiles_obj);
							results.push_back(profile);
						}
					}
					json_decref(json_data);
				}
				else {
					error = EProfileResponseType_GenericError;
				}
			}
		}

		if (json_data) {
			free((void *)json_data);
		}
		if (send_obj)
			json_decref(send_obj);


		request.callback(error, results, users_map, request.extra, request.peer);
	}
	void *ProfileSearchTask::TaskThread(CThread *thread) {
		ProfileSearchTask *task = (ProfileSearchTask *)thread->getParams();
		while (task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			if (task->m_request_list.empty()) {
				task->mp_mutex->unlock();
				break;
			}
			while (!task->m_request_list.empty()) {
				ProfileSearchRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();

				PerformSearch(task_params);

				task->mp_mutex->lock();
				if (task_params.peer)
					task_params.peer->DecRef();
				task->m_request_list.pop();
			}

			task->mp_mutex->unlock();
		}
		return NULL;
	}
	ProfileSearchTask::ProfileSearchTask(int thread_index) : Task<ProfileSearchRequest>() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(ProfileSearchTask::TaskThread, this, true);

	}
	ProfileSearchTask::~ProfileSearchTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	OS::TaskPool<ProfileSearchTask, ProfileSearchRequest> *GetProfileTaskPool() {
		return m_profile_search_task_pool;
	}
	void SetupProfileTaskPool(int num_tasks) {
		m_profile_search_task_pool = new OS::TaskPool<ProfileSearchTask, ProfileSearchRequest>(num_tasks);
	}
	void ShutdownProfileTaskPool() {
		delete m_profile_search_task_pool;
	}
}