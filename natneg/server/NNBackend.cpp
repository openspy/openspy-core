#include "NNBackend.h"

#include <OS/legacy/helpers.h>

namespace NN {
	const char *nn_channel = "natneg.backend";
	NNQueryTask *NNQueryTask::m_task_singleton = NULL;
	void *NNQueryTask::TaskThread(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<NNBackendRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					NNBackendRequest task_params = *it;
					switch(task_params.type) {
						case ENNQueryRequestType_SubmitCookie:
						task->PerformSubmit(task_params);
						break;
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

	void *NNQueryTask::setup_redis_async(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();

		struct event_base *base = event_base_new();

	    task->mp_redis_async_connection = redisAsyncConnect("127.0.0.1", 6379);

	    redisLibeventAttach(task->mp_redis_async_connection, base);
	    redisAsyncCommand(task->mp_redis_async_connection, NNQueryTask::onRedisMessage, thread->getParams(), "SUBSCRIBE %s",nn_channel);
	    event_base_dispatch(base);
		return NULL;
	}

	void NNQueryTask::onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
		NNQueryTask *task = (NNQueryTask *)privdata;
	    redisReply *r = (redisReply*)reply;
	    if (reply == NULL) return;

	    char type[32];
		uint8_t *data_out;
		int data_len;
		char ip_str[32];

	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    			find_param(0, r->element[2]->str,(char *)&type, sizeof(type)-1);
	    			if(!strcmp(type,"natneg_init")) {
	    				find_param("ipstr",r->element[2]->str,(char *)&ip_str, sizeof(ip_str)-1);
	    				int cookie = find_paramint("natneg_init", r->element[2]->str);
	    				int client_idx = find_paramint("index", r->element[2]->str);
	    				OS::Address addr((const char *)&ip_str);
	    				std::vector<NN::Driver *>::iterator it = task->m_drivers.begin();
	    				while(it != task->m_drivers.end()) {
	    					NN::Driver *driver = *it;
	    					driver->OnGotCookie(cookie, client_idx, addr);
	    					it++;
	    				}
	    			}
	    		
	    	}
	    }
	}

	NNQueryTask::NNQueryTask() {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);
		mp_redis_async_retrival_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		OS::CreateThread(setup_redis_async, this, true);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(NNQueryTask::TaskThread, this, true);
	}
	NNQueryTask::~NNQueryTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	NNQueryTask *NNQueryTask::getQueryTask() {
		if(!NNQueryTask::m_task_singleton) {
			NNQueryTask::m_task_singleton = new NNQueryTask();
		}
		return NNQueryTask::m_task_singleton;
	}


	void NNQueryTask::AddDriver(NN::Driver *driver) {
		m_drivers.push_back(driver);
	}
	void NNQueryTask::RemoveDriver(NN::Driver *driver) {

	}

	void NNQueryTask::SubmitClient(Peer *peer) {
		NNBackendRequest task_params;
		task_params.extra = peer;
		task_params.type = ENNQueryRequestType_SubmitCookie;
		task_params.data.cookie = peer->GetCookie();
		task_params.data.client_index = peer->GetClientIndex();
		task_params.data.address = peer->getAddress();
		NNQueryTask::getQueryTask()->AddRequest(task_params);
	}
	void NNQueryTask::PerformSubmit(NNBackendRequest task_params) {

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_NatNeg));

		const struct sockaddr_in addr = task_params.data.address.GetInAddr();
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);

		//freeReplyObject(redisCommand(mp_redis_connection, "HSET nn_cookie_%u %d %s:%d", task_params.data.cookie,task_params.data.client_index,ipinput,task_params.data.address.port));
		//freeReplyObject(redisCommand(mp_redis_connection, "EXPIRE nn_cookie_%u 300", task_params.data.cookie));

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\natneg_init\\%u\\index\\%d\\ipstr\\%s:%d",nn_channel,task_params.data.cookie,task_params.data.client_index,ipinput,task_params.data.address.port));


	}

}