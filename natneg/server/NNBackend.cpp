#include "NNPeer.h"
#include "NNBackend.h"
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>
#define NATNEG_COOKIE_TIME 30 
namespace NN {
	OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool = NULL;
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	Redis::Connection *mp_redis_async_connection;
	NNQueryTask *mp_async_lookup_task = NULL;

	const char *nn_channel = "natneg.backend";

	void *NNQueryTask::TaskThread(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();
		while(!task->m_request_list.empty() || task->mp_thread_poller->wait()) {
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

		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), nn_channel) == 0) {
					std::map<std::string, std::string> kv_data = OS::KeyStringToMap(v.arr_value.values[2].second.value._str.c_str());
					if(kv_data.find("natneg_init") != kv_data.end()) {
						//if (kv_data["type"].compare("") == 0) {
						NNCookieType cookie = (NNCookieType)atoi(kv_data["natneg_init"].c_str());
						int client_idx = atoi(kv_data["index"].c_str());
						OS::Address addr(kv_data["ipstr"].c_str());
						OS::Address private_addr(kv_data["privateip"].c_str());
						server->OnGotCookie(cookie, client_idx, addr, private_addr);
					}
				end_exit:
					return;
				}
			}
		}
	}

	NNQueryTask::NNQueryTask(int thread_id) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		m_thread_id = thread_id;
		m_thread_awake = false;

		mp_redis_connection = Redis::Connect(OS::g_redisAddress, t);

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
		Redis::Command(mp_redis_connection, 0, "PUBLISH %s '\\natneg_erttest\\%s\\type\\%d'", nn_channel, address.ToString().c_str(),task_params.extra);
	}
	void NNQueryTask::PerformSubmit(NNBackendRequest task_params) {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		OS::Address address = task_params.peer->getAddress();
		int client_index = task_params.peer->GetClientIndex();
		int search_index = client_index == 1 ? 0 : 1;

		std::string nn_key;
		std::ostringstream nn_key_ss;
		nn_key_ss << "nn_cookie_" << task_params.peer->GetCookie();
		nn_key = nn_key_ss.str();

		//Check for existing natneg cookies, and send if already sent by client
		Redis::Response reply = Redis::Command(mp_redis_connection, 0, "EXISTS %s", nn_key.c_str());
		if (!(reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)) { 
			std::string connect_address;
			reply = Redis::Command(mp_redis_connection, 0, "HGET %s %d", nn_key.c_str(), search_index);
			if (reply.values.size() > 0 && reply.values.front().type != Redis::REDIS_RESPONSE_TYPE_ERROR) {
				Redis::Value v = reply.values.front();
				std::vector<std::string> ip_list = OS::KeyStringToVector(v.value._str);
				if (ip_list.size() == 2) {
					OS::Address peer_address(ip_list.at(0).c_str()), private_peer_address(ip_list.at(1).c_str());
					task_params.peer->OnGotPeerAddress(peer_address, private_peer_address);
				}
			}
			//Redis::Command(mp_redis_connection, 0, "HDEL %s %d", nn_key.c_str(), search_index); //delete cookie to prevent re-sends when invalid
		}
			

		Redis::Command(mp_redis_connection, 0, "HSET %s %d '\\%s\\%s'", nn_key.c_str(), client_index, address.ToString().c_str(), task_params.peer->getPrivateAddress().ToString().c_str());
		Redis::Command(mp_redis_connection, 0, "EXPIRE %s %d", nn_key.c_str(), NATNEG_COOKIE_TIME);
		switch (task_params.type) {
			case NN::ENNQueryRequestType_SubmitClient:
				Redis::Command(mp_redis_connection, 0, "PUBLISH %s '\\natneg_init\\%d\\index\\%d\\ipstr\\%s\\gamename\\%s\\privateip\\%s'", nn_channel, task_params.peer->GetCookie(), task_params.peer->GetClientIndex(), address.ToString().c_str(), task_params.peer->getGamename().c_str(), task_params.peer->getPrivateAddress().ToString().c_str());
				break;
		}

	}

	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 1;
		mp_redis_async_connection = Redis::Connect(OS::g_redisAddress, t);
		mp_async_lookup_task = new NNQueryTask(NUM_NN_QUERY_THREADS+1);
		Redis::LoopingCommand(mp_redis_async_connection, 0, onRedisMessage, thread->getParams(), "SUBSCRIBE %s", nn_channel);
		return NULL;
	}
	void SetupTaskPool(NN::Server* server) {

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_redis_async_retrival_connection = Redis::Connect(OS::g_redisAddress, t);
		mp_async_thread = OS::CreateThread(setup_redis_async, server, true);

		m_task_pool = new OS::TaskPool<NNQueryTask, NNBackendRequest>(NUM_NN_QUERY_THREADS);
		server->SetTaskPool(m_task_pool);
	}
	void Shutdown() {
	}

}
