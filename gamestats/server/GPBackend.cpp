#include <stdio.h>
#include "GPBackend.h"
#include "GPDriver.h"
#include <stdlib.h>
#include <string.h>
#include <OS/socketlib/socketlib.h>



#include <OS/OpenSpy.h>
#include <OS/Thread.h>
#include <OS/HTTP.h>

#include <OS/legacy/helpers.h>

#include <sstream>

#include <OS/Search/Profile.h>
#include <curl/curl.h>
#include <jansson.h>
#include <jwt.h>

#define GP_BACKEND_REDIS_DB 5
#define BUDDY_ADDREQ_EXPIRETIME 604800
#define GP_STATUS_EXPIRE_TIME 3600

#define GP_PERSIST_BACKEND_URL  OPENSPY_WEBSERVICES_URL "/backend/persist"
#define GP_PERSIST_BACKEND_CRYPTKEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

namespace GPBackend {
	struct curl_data {
	    //json_t *json_data;
	    std::string buffer;
	};

	PersistBackendTask *PersistBackendTask::m_task_singleton = NULL;

	const char *gp_buddies_channel = "persist";
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

	void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
	    redisReply *r = (redisReply*)reply;

	    if (reply == NULL) return;
	    int to_profileid, from_profileid;
	    char msg_type[16], reason[GP_REASON_LEN + 1];
	    GP::Peer *peer = NULL;
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		if(strcmp(r->element[1]->str,gp_buddies_channel) == 0) {
	    			if(!find_param("type", r->element[2]->str,(char *)&msg_type, sizeof(msg_type))) {
	    					return;
	    			}
	    		}
	    	}
	    }
	}
	void *PersistBackendTask::setup_redis_async_sub(OS::CThread *thread) {
		PersistBackendTask *task = (PersistBackendTask *)thread->getParams();
		task->mp_event_base = event_base_new();

	    task->mp_redis_subscribe_connection = redisAsyncConnect("127.0.0.1", 6379);
	    
	    redisLibeventAttach(task->mp_redis_subscribe_connection, task->mp_event_base);

	    redisAsyncCommand(task->mp_redis_subscribe_connection, onRedisMessage, NULL, "SUBSCRIBE %s",gp_buddies_channel);

	    event_base_dispatch(task->mp_event_base);
	}

	PersistBackendTask::PersistBackendTask() {
		mp_redis_async_thread = OS::CreateThread(setup_redis_async_sub, this, true);

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(PersistBackendTask::TaskThread, this, true);
	}
	PersistBackendTask::~PersistBackendTask() {
		delete mp_thread;
		delete mp_redis_async_thread;
		delete mp_mutex;

		event_base_free(mp_event_base);

		redisFree(mp_redis_connection);
		redisAsyncFree(mp_redis_subscribe_connection);
	}
	void PersistBackendTask::Shutdown() {
		PersistBackendTask *task = getPersistBackendTask();
		
		delete task;	
	}
	PersistBackendTask *PersistBackendTask::getPersistBackendTask() {
		if(!PersistBackendTask::m_task_singleton) {
			PersistBackendTask::m_task_singleton = new PersistBackendTask();
		}
		return PersistBackendTask::m_task_singleton;
	}
	void PersistBackendTask::SubmitNewGameSession(GP::Peer *peer, void* extra, PersistBackendCallback cb) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_NewGame;
		req.callback = cb;
		getPersistBackendTask()->AddRequest(req);
	}
	void PersistBackendTask::SubmitUpdateGameSession(std::map<std::string, std::string> kvMap, GP::Peer *peer, void* extra, std::string game_instance_identifier, PersistBackendCallback cb) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_UpdateGame;
		req.game_instance_identifier = game_instance_identifier;
		req.callback = cb;
		req.kvMap = kvMap;
		getPersistBackendTask()->AddRequest(req);
	}
	void PersistBackendTask::PerformNewGameSession(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.mp_peer->GetProfileID()));
		json_object_set_new(send_json, "type", json_string("newgame"));

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		OS::HTTPResponse resp =	client.Post(jwt_encoded);

		jwt_free(jwt);
		free((void *)jwt_encoded);
		free(json_data);
		json_decref(send_json);

		//build jwt
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)GP_PERSIST_BACKEND_CRYPTKEY, strlen(GP_PERSIST_BACKEND_CRYPTKEY));

		//int jwt_decode(jwt_t **jwt, const char *token, const unsigned char *key,int key_len);
		jwt_decode(&jwt, resp.buffer.c_str(), NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		send_json = json_loads(json, 0, NULL);


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

		json_decref(send_json);

		
	}
	void PersistBackendTask::PerformUpdateGameSession(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.mp_peer->GetProfileID()));
		json_object_set_new(send_json, "type", json_string("updategame"));
		json_object_set_new(send_json, "game_identifier", json_string(req.game_instance_identifier.c_str()));

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

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		OS::HTTPResponse resp =	client.Post(jwt_encoded);

		jwt_free(jwt);
		free((void *)jwt_encoded);
		free(json_data);
		json_decref(send_json);

		//build jwt
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)GP_PERSIST_BACKEND_CRYPTKEY, strlen(GP_PERSIST_BACKEND_CRYPTKEY));

		//int jwt_decode(jwt_t **jwt, const char *token, const unsigned char *key,int key_len);
		jwt_decode(&jwt, resp.buffer.c_str(), NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		send_json = json_loads(json, 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_UpdateGame;
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);

	}
	void PersistBackendTask::SubmitSetPersistData(int profileid, GP::Peer *peer, void* extra, PersistBackendCallback cb, std::string data_b64_buffer, persisttype_t type, int index, bool kv_set) {
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
		getPersistBackendTask()->AddRequest(req);
	}
	void PersistBackendTask::SubmitGetPersistData(int profileid, GP::Peer *peer, void *extra, PersistBackendCallback cb, persisttype_t type, int index, std::vector<std::string> keyList) {
		PersistBackendRequest req;
		req.mp_peer = peer;
		req.mp_extra = extra;
		req.type = EPersistRequestType_GetUserData;
		req.callback = cb;
		req.profileid = profileid;
		req.data_type = type;
		req.data_index = index;
		req.keyList = keyList;
		getPersistBackendTask()->AddRequest(req);
	}
	void PersistBackendTask::PerformSetPersistData(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.profileid));
		json_object_set_new(send_json, "type", json_string("set_persist_data"));


		json_object_set_new(send_json, "data", json_string(req.game_instance_identifier.c_str()));
		json_object_set_new(send_json, "data_index", json_integer(req.data_index));
		json_object_set_new(send_json, "data_type", json_integer(req.data_type));

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		OS::HTTPResponse resp =	client.Post(jwt_encoded);

		jwt_free(jwt);
		free((void *)jwt_encoded);
		free(json_data);
		json_decref(send_json);

		//build jwt
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)GP_PERSIST_BACKEND_CRYPTKEY, strlen(GP_PERSIST_BACKEND_CRYPTKEY));

		//int jwt_decode(jwt_t **jwt, const char *token, const unsigned char *key,int key_len);
		jwt_decode(&jwt, resp.buffer.c_str(), NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		send_json = json_loads(json, 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_SetUserData;
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);		
	}
	void PersistBackendTask::PerformGetPersistData(PersistBackendRequest req) {
		json_t *send_json = json_object();

		json_object_set_new(send_json, "profileid", json_integer(req.profileid));
		json_object_set_new(send_json, "type", json_string("get_persist_data"));


		json_object_set_new(send_json, "data_index", json_integer(req.data_index));
		json_object_set_new(send_json, "data_type", json_integer(req.data_type));

		std::vector<std::string>::iterator it = req.keyList.begin();

		json_t *keylist_array = json_array();
		while(it != req.keyList.end()) {
			json_array_append(keylist_array, json_string((*it).c_str()));
			it++;
		}
		json_object_set(send_json, "keyList", keylist_array);

		OS::HTTPClient client(GP_PERSIST_BACKEND_URL);

		char *json_data = json_dumps(send_json, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_AUTH_KEY, strlen(OPENSPY_AUTH_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		OS::HTTPResponse resp =	client.Post(jwt_encoded);

		jwt_free(jwt);
		free((void *)jwt_encoded);
		free(json_data);
		json_decref(send_json);

		//build jwt
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)GP_PERSIST_BACKEND_CRYPTKEY, strlen(GP_PERSIST_BACKEND_CRYPTKEY));

		//int jwt_decode(jwt_t **jwt, const char *token, const unsigned char *key,int key_len);
		jwt_decode(&jwt, resp.buffer.c_str(), NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		send_json = json_loads(json, 0, NULL);


		json_t *success_obj = json_object_get(send_json, "success");
		bool success = success_obj == json_true();

		json_t *data_obj = json_object_get(send_json, "data");


		PersistBackendResponse resp_data;
		resp_data.type = EPersistBackendRespType_GetUserData;

		if(data_obj) {
			json_t *persist_obj = json_object_get(data_obj, "data");
			if(persist_obj) {
				resp_data.game_instance_identifier = json_string_value(persist_obj);
			}
		}
		req.callback(success, resp_data, req.mp_peer, req.mp_extra);

		json_decref(send_json);	
	}
	void *PersistBackendTask::TaskThread(OS::CThread *thread) {
		PersistBackendTask *task = (PersistBackendTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<PersistBackendRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					PersistBackendRequest task_params = *it;
					if(GP::g_gbl_gp_driver->HasPeer(task_params.mp_peer)) {
					
						switch(task_params.type) {
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
					}
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(TASK_SLEEP_TIME);
		}
		return NULL;
	}
}