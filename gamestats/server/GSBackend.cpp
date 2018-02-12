#include <stdio.h>
#include <algorithm>
#include <OS/OpenSpy.h>
#include "GSBackend.h"
#include "GSDriver.h"
#include "GSServer.h"
#include <stdlib.h>
#include <string.h>


#include <OS/OpenSpy.h>
#include <OS/Thread.h>
#include <OS/HTTP.h>

#include <OS/legacy/helpers.h>

#include <sstream>

#include <OS/Search/Profile.h>
#include <curl/curl.h>
#include <jansson.h>

#define GP_BACKEND_REDIS_DB 5
#define BUDDY_ADDREQ_EXPIRETIME 604800
#define GP_STATUS_EXPIRE_TIME 3600

#define GP_PERSIST_BACKEND_URL  OPENSPY_WEBSERVICES_URL "/backend/persist"
#define GP_PERSIST_BACKEND_CRYPTKEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

namespace GSBackend {
	struct curl_data {
	    //json_t *json_data;
	    std::string buffer;
	};

	OS::TaskPool<PersistBackendTask, PersistBackendRequest> *m_task_pool = NULL;

	/* callback for curl fetch */
	size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

	    data->buffer = data->buffer + (const char *)contents;

	    return realsize;
	}

	PersistBackendTask::PersistBackendTask(int thread_index) {
		m_thread_index = thread_index;
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(PersistBackendTask::TaskThread, this, true);
	}
	PersistBackendTask::~PersistBackendTask() {
		delete mp_thread;
		delete mp_mutex;
	}
	void PersistBackendTask::SubmitNewGameSession(GS::Peer *peer, void* extra, PersistBackendCallback cb) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_NewGame;
		req.callback = cb;
		peer->IncRef();
		m_task_pool->AddRequest(req);
	}
	void PersistBackendTask::SubmitUpdateGameSession(std::map<std::string, std::string> kvMap, GS::Peer *peer, void* extra, std::string game_instance_identifier, PersistBackendCallback cb) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_UpdateGame;
		req.game_instance_identifier = game_instance_identifier;
		req.callback = cb;
		req.kvMap = kvMap;

		peer->IncRef();
		m_task_pool->AddRequest(req);
	}
	void PersistBackendTask::PerformNewGameSession(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.mp_peer->GetProfileID()));
		json_object_set_new(send_json, "mode", json_string("newgame"));
		json_object_set_new(send_json, "game_id", json_integer(req.mp_peer->GetGame().gameid));

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp =	client.Post(json_data);


		send_json = json_loads(resp.buffer.c_str(), 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();
		PersistBackendResponse resp_data;
		if(success_obj == json_true()) {
			json_t *data_json = json_object_get(send_json, "data");

			json_t *game_identifier_json = json_object_get(data_json, "game_identifier");

			resp_data.game_instance_identifier = json_string_value(game_identifier_json);
			resp_data.type = EPersistBackendRespType_NewGame;

			req.callback(success, resp_data, req.mp_peer, req.mp_extra);
		}
		free((void *)json_data);
		json_decref(send_json);


	}
	void PersistBackendTask::PerformUpdateGameSession(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.mp_peer->GetProfileID()));
		json_object_set_new(send_json, "mode", json_string("updategame"));
		json_object_set_new(send_json, "game_identifier", json_string(req.game_instance_identifier.c_str()));
		json_object_set_new(send_json, "game_id", json_integer(req.mp_peer->GetGame().gameid));

		json_t *data_obj = json_object();
		std::map<std::string, std::string>::iterator it = req.kvMap.begin();
		while(it != req.kvMap.end()) {
			std::pair<std::string, std::string> p = *it;
			json_object_set_new(data_obj, p.first.c_str(), json_string(p.second.c_str()));
			it++;
		}

		json_object_set_new(send_json, "data", data_obj);

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);


		OS::HTTPResponse resp =	client.Post(json_data);
		
		free(json_data);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_UpdateGame;
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);

	}
	void PersistBackendTask::SubmitSetPersistData(int profileid, GS::Peer *peer, void* extra, PersistBackendCallback cb, std::string data_b64_buffer, persisttype_t type, int index, bool kv_set, OS::KVReader kv_set_data) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_SetUserData;
		req.game_instance_identifier = data_b64_buffer;
		req.callback = cb;
		req.data_type = type;
		req.data_index = index;
		req.data_kv_set = kv_set;
		req.profileid = profileid;
		req.kv_set_data = kv_set_data;
		peer->IncRef();
		m_task_pool->AddRequest(req);
	}
	void PersistBackendTask::SubmitGetPersistData(int profileid, GS::Peer *peer, void *extra, PersistBackendCallback cb, persisttype_t type, int index, std::vector<std::string> keyList) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_GetUserData;
		req.callback = cb;
		req.profileid = profileid;
		req.data_type = type;
		req.data_index = index;
		req.keyList = keyList;
		peer->IncRef();
		m_task_pool->AddRequest(req);
	}
	void PersistBackendTask::PerformSetPersistData(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.profileid));
		json_object_set_new(send_json, "mode", json_string("set_persist_data"));


		json_object_set_new(send_json, "data", json_string(req.game_instance_identifier.c_str()));
		json_object_set_new(send_json, "data_index", json_integer(req.data_index));
		json_object_set_new(send_json, "data_type", json_integer(req.data_type));
		json_object_set_new(send_json, "game_id", json_integer(req.mp_peer->GetGame().gameid));

		json_object_set_new(send_json, "kv_set", req.data_kv_set ? json_true() : json_false());

		if (req.data_kv_set) {
			json_t *key_obj = json_object();
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it = req.kv_set_data.GetHead();
			bool found_end = false;
			std::ostringstream ss;
			while (it.first != it.second) {
				std::pair< std::string, std::string> item = *it.first;

				json_object_set_new(key_obj, item.first.c_str(), json_string(item.second.c_str()));
				it.first++;
			}

			json_object_set_new(send_json, "keyList", key_obj);
		}

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);


		OS::HTTPResponse resp =	client.Post(json_data);

		free(json_data);
		json_decref(send_json);
		
		send_json = json_loads(resp.buffer.c_str(), 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_SetUserData;

		success_obj = json_object_get(send_json, "modified_time");
		if (success_obj) {
			resp_data.mod_time = json_integer_value(success_obj);
		}
		else {
			resp_data.mod_time = 0;
		}
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);
	}
	void PersistBackendTask::PerformGetPersistData(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.profileid));
		json_object_set_new(send_json, "mode", json_string("get_persist_data"));


		json_object_set_new(send_json, "data_index", json_integer(req.data_index));
		json_object_set_new(send_json, "data_type", json_integer(req.data_type));
		json_object_set_new(send_json, "game_id", json_integer(req.mp_peer->GetGame().gameid));

		std::vector<std::string>::iterator it = req.keyList.begin();

		json_t *keylist_array = json_array();
		while(it != req.keyList.end()) {
			json_array_append(keylist_array, json_string((*it).c_str()));
			it++;
		}
		json_object_set(send_json, "keyList", keylist_array);

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp =	client.Post(json_data);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		json_t *data_obj = json_object_get(send_json, "data");


		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_GetUserData;

		if(data_obj) {
			resp_data.game_instance_identifier = json_string_value(data_obj);
		}

		data_obj = json_object_get(send_json, "keyList");

		const char *key;
		json_t *value;

		if (data_obj) {
			std::ostringstream ss;
			json_object_foreach(data_obj, key, value) {
				ss << "\\" << key << "\\" << json_string_value(value);				
			}
			resp_data.kv_data = ss.str();
		}

		success_obj = json_object_get(send_json, "modified_time");
		if (success_obj) {
			resp_data.mod_time = json_integer_value(success_obj);
		}
		else {
			resp_data.mod_time = 0;
		}
		
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);
	}
	void *PersistBackendTask::TaskThread(OS::CThread *thread) {
		PersistBackendTask *task = (PersistBackendTask *)thread->getParams();
		while (!task->m_request_list.empty() || task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			while (!task->m_request_list.empty()) {
				PersistBackendRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();

				switch (task_params.type) {
					case EPersistRequestType_NewGame:
						task->PerformNewGameSession(task_params);
						break;
					case EPersistRequestType_UpdateGame:
						task->PerformUpdateGameSession(task_params);
						break;
					case EPersistRequestType_SetUserData:
						task->PerformSetPersistData(task_params);
						break;
					case EPersistRequestType_GetUserData:
						task->PerformGetPersistData(task_params);
						break;
				}

				task->mp_mutex->lock();
				task_params.mp_peer->DecRef();
				task->m_request_list.pop();
			}
			task->mp_mutex->unlock();
		}
		return NULL;
	}
	void PersistBackendTask::AddDriver(GS::Driver *driver) {
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
		}
	}
	void PersistBackendTask::RemoveDriver(GS::Driver *driver) {
		std::vector<GS::Driver *>::iterator it = std::find(m_drivers.begin(), m_drivers.end(), driver);
		if (it != m_drivers.end()) {
			it = m_drivers.erase(it);
		}
	}
	void SetupTaskPool(GS::Server *server) {
		OS::Sleep(200);

		m_task_pool = new OS::TaskPool<PersistBackendTask, PersistBackendRequest>(NUM_STATS_THREADS);
		server->SetTaskPool(m_task_pool);
	}
	void ShutdownTaskPool() {

	}

}
