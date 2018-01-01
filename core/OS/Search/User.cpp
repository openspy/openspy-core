#include <OS/OpenSpy.h>
#include <OS/Search/User.h>

#include <curl/curl.h>
#include <jansson.h>

namespace OS {
	OS::TaskPool<UserSearchTask, UserSearchRequest> *m_user_search_task_pool = NULL;
	struct curl_data {
		std::string buffer;
	};
	/* callback for curl fetch */
	size_t UserSearchTask::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if (!contents) {
			return 0;
		}
		size_t realsize = size * nmemb;                             /* calculate buffer size */
		curl_data *data = (curl_data *)userp;
		data->buffer += (const char *)contents;
		return realsize;
	}
	void UserSearchTask::PerformRequest(UserSearchRequest request) {
		curl_data recv_data;
		json_t *root = NULL;
		std::vector<OS::User> results;
		//build json object
		json_t *send_obj = json_object();

		switch(request.type) {
			case EUserRequestType_Update:
				json_object_set_new(send_obj, "mode", json_string("update_user"));	
			break;
			case EUserRequestType_Search:
				json_object_set_new(send_obj, "mode", json_string("get_user"));	
			break;
		}

		json_t *user_obj = json_object();

		if(request.search_params.id != 0) {
			json_object_set_new(user_obj, "id", json_integer(request.search_params.id));
		}

		//user parameters
		if(request.search_params.email.length())
			json_object_set_new(user_obj, "email", json_string(request.search_params.email.c_str()));


		if(request.type == EUserRequestType_Update) {
			if(request.search_params.password.length())
				json_object_set_new(user_obj, "password", json_string(request.search_params.password.c_str()));

			json_object_set_new(user_obj, "videocard1ram", json_integer(request.search_params.videocard_ram[0]));
			json_object_set_new(user_obj, "videocard2ram", json_integer(request.search_params.videocard_ram[1]));

			json_object_set_new(user_obj, "cpuspeed", json_integer(request.search_params.cpu_speed));
			json_object_set_new(user_obj, "cpubrandid", json_integer(request.search_params.cpu_brandid));
			json_object_set_new(user_obj, "connectionspeed", json_integer(request.search_params.connectionspeed));
			json_object_set_new(user_obj, "hasnetwork", request.search_params.hasnetwork ? json_true() : json_false());
			json_object_set_new(user_obj, "publicmask", json_integer(request.search_params.publicmask));
		}		
		json_object_set_new(user_obj, "partnercode", json_integer(request.search_params.partnercode));

		json_object_set_new(send_obj, "user", user_obj);


		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		EUserResponseType resp_type = EUserResponseType_Success;

		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_USERMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchUser");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			/* Close socket after one use */
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			if(res == CURLE_OK) {
				root = json_loads(recv_data.buffer.c_str(), 0, NULL);
				json_t *user_obj = json_object_get(root, "user");
				if(user_obj) {
					OS::User user = OS::LoadUserFromJson(user_obj);
					results.push_back(user);
				}
				resp_type = EUserResponseType_Success;
			} else {
				resp_type = EUserResponseType_GenericError;
			}
		}
		
		if(root) {
			json_decref(root);
		}

		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);


		if(request.callback != NULL)
			request.callback(resp_type, results, request.extra, request.peer);
	}

	void *UserSearchTask::TaskThread(CThread *thread) {
		UserSearchTask *task = (UserSearchTask *)thread->getParams();
		while (!task->m_request_list.empty() || task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			while (!task->m_request_list.empty()) {
				UserSearchRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();
				switch(task_params.type) {
					case EUserRequestType_Update:
					case EUserRequestType_Search:
						PerformRequest(task_params);
					break;	
				}
				if (task_params.peer)
					task_params.peer->DecRef();

				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			task->mp_mutex->unlock();
		}
		return NULL;
	}
	UserSearchTask::UserSearchTask(int thread_index) : Task<UserSearchRequest>() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(UserSearchTask::TaskThread, this, true);

	}
	UserSearchTask::~UserSearchTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	void SetupUserSearchTaskPool(int num_tasks) {
		m_user_search_task_pool = new OS::TaskPool<UserSearchTask, UserSearchRequest>(num_tasks);
	}
	void ShutdownUserSearchTaskPool() {
		delete m_user_search_task_pool;
	}
}