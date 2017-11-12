#include "NNPeer.h"
#include "NNBackend.h"
#include <algorithm>
#include <OS/legacy/helpers.h>

#include <OS/TaskPool.h>
#include <OS/Redis.h>
#define NATNEG_COOKIE_TIME 300 
namespace NN {
	OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool = NULL;
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	Redis::Connection *mp_redis_async_connection;
	NNQueryTask *mp_async_lookup_task = NULL;

	const char *nn_channel = "natneg.backend";

	void *NNQueryTask::TaskThread(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();
		while(task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			task->m_thread_awake = true;
			while (!task->m_request_list.empty()) {
				NNBackendRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();
				task->mp_timer->start();
				switch (task_params.type) {
					case ENNQueryRequestType_SubmitClient:
						task->PerformSubmit(task_params);
						break;
					case ENNQueryRequestType_PerformERTTest:
						task->PerformERTTest(task_params);
						break;
				}
				task->mp_timer->stop();
				if (task_params.peer) {
					OS::LogText(OS::ELogLevel_Info, "[%s] Thread type %d - time: %f", OS::Address(task_params.peer->getAddress()).ToString().c_str(), task_params.type, task->mp_timer->time_elapsed() / 1000000.0);
				}
				task_params.peer->DecRef();
				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			task->m_thread_awake = false;
			task->mp_mutex->unlock();
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
						NNCookieType cookie = (NNCookieType)find_paramint("natneg_init", temp_str);
						int client_idx = find_paramint("index", temp_str);
						OS::Address addr((const char *)&ip_str);

						find_param("privateip", temp_str, (char *)&ip_str, sizeof(ip_str) - 1);
						OS::Address private_addr((const char *)&ip_str);

						OS::LogText(OS::ELogLevel_Info, "Async got cookie: %d - %d - %s - %s", cookie, client_idx, addr.ToString(), private_addr.ToString());
						server->OnGotCookie(cookie, client_idx, addr, private_addr);
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

		m_thread_awake = false;

		mp_redis_connection = Redis::Connect(OS_REDIS_ADDR, t);

		mp_timer = OS::HiResTimer::makeTimer();
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
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
		}
		if (this != mp_async_lookup_task && mp_async_lookup_task) {
			mp_async_lookup_task->AddDriver(driver);
		}
	}
	void NNQueryTask::RemoveDriver(NN::Driver *driver) {
		std::vector<NN::Driver *>::iterator it = std::find(m_drivers.begin(), m_drivers.end(), driver);
		if (it != m_drivers.end()) {
			m_drivers.erase(it);
		}
	}
	void NNQueryTask::PerformERTTest(NNBackendRequest task_params) {
		OS::Address address = task_params.peer->getAddress();
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);
		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\natneg_erttest\\%s\\type\\%d", nn_channel, address.ToString().c_str(),task_params.extra);
	}
	void NNQueryTask::PerformSubmit(NNBackendRequest task_params) {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		OS::Address address = task_params.peer->getAddress();

		Redis::Command(mp_redis_connection, 0, "HSET nn_cookie_%d %d %s", task_params.peer->GetCookie(),task_params.peer->GetClientIndex(), address.ToString().c_str());
		Redis::Command(mp_redis_connection, 0, "EXPIRE nn_cookie_%d %d", task_params.peer->GetCookie(), NATNEG_COOKIE_TIME);
		switch (task_params.type) {
			case NN::ENNQueryRequestType_SubmitClient:
				Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\natneg_init\\%d\\index\\%d\\ipstr\\%s\\gamename\\%s\\privateip\\%s", nn_channel, task_params.peer->GetCookie(), task_params.peer->GetClientIndex(), address.ToString().c_str(), task_params.peer->getGamename().c_str(), task_params.peer->getPrivateAddress().ToString().c_str());
				
				break;
		}

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
