#include "NNBackend.h"

#include <OS/legacy/helpers.h>

#include <OS/TaskPool.h>
#include <OS/Redis.h>
namespace NN {
	OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool = NULL;
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	Redis::Connection *mp_redis_async_connection;
	NNQueryTask *mp_async_lookup_task = NULL;

	const char *nn_channel = "natneg.backend";
	void *NNQueryTask::TaskThread(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();
		for(;;) {
			task->mp_mutex->lock();
			while (!task->m_request_list.empty()) {
				NNBackendRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();

				switch (task_params.type) {
					case ENNQueryRequestType_SubmitCookie:
						task->PerformSubmit(task_params);
						break;
				}

				task_params.peer->DecRef();
				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			task->mp_mutex->unlock();
			OS::Sleep(TASK_SLEEP_TIME);
		}
		return NULL;
	}

	void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
		Redis::Value v = reply.values.front();
		NN::Server *server = (NN::Server *)privdata;

		char type[32];
		char ip_str[32];


		char msg_type[16], server_key[64];
		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), nn_channel) == 0) {
					char *temp_str = strdup(v.arr_value.values[2].second.value._str.c_str());
					find_param(0, temp_str, (char *)&type, sizeof(type) - 1);
					if (!strcmp(type, "natneg_init")) {
						find_param("ipstr", temp_str, (char *)&ip_str, sizeof(ip_str) - 1);
						int cookie = find_paramint("natneg_init", temp_str);
						int client_idx = find_paramint("index", temp_str);
						OS::Address addr((const char *)&ip_str);
						server->OnGotCookie(cookie, client_idx, addr);
					}
				end_exit:
					free((void *)temp_str);
				}
			}
		}
	}

	NNQueryTask::NNQueryTask() {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_connection = Redis::Connect(OS_REDIS_ADDR, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(NNQueryTask::TaskThread, this, true);
	}
	NNQueryTask::~NNQueryTask() {
		delete mp_thread;
		delete mp_mutex;

		Redis::Disconnect(mp_redis_connection);
	}
	void NNQueryTask::Shutdown() {

	}


	void NNQueryTask::AddDriver(NN::Driver *driver) {
		m_drivers.push_back(driver);
	}
	void NNQueryTask::RemoveDriver(NN::Driver *driver) {

	}

	void NNQueryTask::PerformSubmit(NNBackendRequest task_params) {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		OS::Address address = task_params.data.address;

		Redis::Command(mp_redis_connection, 0, "HSET nn_cookie_%u %d %s", task_params.data.cookie,task_params.data.client_index, address.ToString());
		Redis::Command(mp_redis_connection, 0, "EXPIRE nn_cookie_%u 300", task_params.data.cookie);
		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\natneg_init\\%u\\index\\%d\\ipstr\\%s", nn_channel, task_params.data.cookie, task_params.data.client_index, address.ToString());

	}

	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 1;
		mp_redis_async_connection = Redis::Connect(OS_REDIS_ADDR, t);
		mp_async_lookup_task = new NNQueryTask();
		Redis::LoopingCommand(mp_redis_async_connection, 0, onRedisMessage, thread->getParams(), "SUBSCRIBE %s", nn_channel);
		return NULL;
	}
	void SetupTaskPool(NN::Server* server) {

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_async_retrival_connection = Redis::Connect(OS_REDIS_ADDR, t);
		mp_async_thread = OS::CreateThread(setup_redis_async, server, true);

		m_task_pool = new OS::TaskPool<NNQueryTask, NNBackendRequest>(NUM_NN_QUERY_THREADS);
		server->SetTaskPool(m_task_pool);
	}
	void Shutdown() {
	}

}
