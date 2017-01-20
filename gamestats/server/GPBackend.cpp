#include <stdio.h>
#include "GPBackend.h"
#include "GPDriver.h"
#include <stdlib.h>
#include <string.h>
#include <OS/socketlib/socketlib.h>

#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/hiredis.h>
#include <hiredis/adapters/libevent.h>
#include <event.h>
#undef _WINSOCK2API_

#include <OS/OpenSpy.h>
#include <OS/Thread.h>

#include <OS/legacy/helpers.h>

#include <sstream>

#include <OS/Search/Profile.h>
#include <curl/curl.h>
#include <jansson.h>
#include <jwt/jwt.h>

#define GP_BACKEND_REDIS_DB 5
#define BUDDY_ADDREQ_EXPIRETIME 604800
#define GP_STATUS_EXPIRE_TIME 3600


namespace GPBackend {
	struct curl_data {
	    //json_t *json_data;
	    std::string buffer;
	};

	redisContext *mp_redis_connection = NULL;
	redisAsyncContext *mp_redis_subscribe_connection = NULL;

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
	void *setup_redis_async_sub(OS::CThread *) {
		struct event_base *base_sub = event_base_new();

	    mp_redis_subscribe_connection = redisAsyncConnect("127.0.0.1", 6379);
	    
	    redisLibeventAttach(mp_redis_subscribe_connection, base_sub);

	    redisAsyncCommand(mp_redis_subscribe_connection, onRedisMessage, NULL, "SUBSCRIBE %s",gp_buddies_channel);

	    event_base_dispatch(base_sub);
	}

	PersistBackendTask::PersistBackendTask() {
		OS::CreateThread(setup_redis_async_sub, NULL, true);

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(PersistBackendTask::TaskThread, this, true);
	}
	PersistBackendTask::~PersistBackendTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	PersistBackendTask *PersistBackendTask::getPersistBackendTask() {
		if(!PersistBackendTask::m_task_singleton) {
			PersistBackendTask::m_task_singleton = new PersistBackendTask();
		}
		return PersistBackendTask::m_task_singleton;
	}
	void *PersistBackendTask::TaskThread(OS::CThread *thread) {
		/*
		GPBackendRedisTask *task = (GPBackendRedisTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<GPBackendRedisRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					GPBackendRedisRequest task_params = *it;
					switch(task_params.type) {
					}
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(TASK_SLEEP_TIME);
		}
		return NULL;
		*/
	}
}