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
		while (thread->isRunning() && (!task->m_request_list.empty() || task->mp_thread_poller->wait()) && thread->isRunning()) {
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
					case ENNQueryRequestType_PerformERTTest_IPUnsolicited:
					case ENNQueryRequestType_PerformERTTest_IPPortUnsolicited:
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

	void NNQueryTask::onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
		Redis::Value v = reply.values.front();
		NN::Server *server = (NN::Server *)privdata;

		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), nn_channel) == 0) {
					std::map<std::string, std::string> kv_data = OS::KeyStringToMap(v.arr_value.values[2].second.value._str.c_str());
					if(kv_data.find("msg") != kv_data.end()) {
						std::string msg = kv_data["msg"];
						if (msg.compare("final_peer") == 0) {
							NNCookieType cookie = (NNCookieType)atoi(kv_data["cookie"].c_str());
							int client_idx = atoi(kv_data["client_index"].c_str());
							int opposite_client_idx = client_idx == 0 ? 1 : 0;
							std::vector<NN::Peer *> peer_list = server->FindConnections(cookie, opposite_client_idx);
							std::vector<NN::Peer *>::iterator it = peer_list.begin();
							std::ostringstream nn_key_ss;
							nn_key_ss << "nn_client_" << client_idx << "_" << cookie;

							ConnectionSummary summary = mp_async_lookup_task->LoadConnectionSummary(nn_key_ss.str());
							NAT nat;
							OS::Address next_public_address, next_private_address;
							NN::LoadSummaryIntoNAT(summary, nat);
							NN:DetermineNatType(nat);
							NN::DetermineNextAddress(nat, next_public_address, next_private_address);
							
							while (it != peer_list.end()) {
								NN::Peer *peer = *it;
								peer->OnGotPeerAddress(next_public_address, next_private_address, nat);
								it++;
							}
						}
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
		mp_thread->SignalExit(true, mp_thread_poller);

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
		Redis::Command(mp_redis_connection, 0, "PUBLISH %s '\\msg\\natneg_erttest\\address\\%s\\type\\%d'", nn_channel, address.ToString().c_str(), task_params.type == ENNQueryRequestType_PerformERTTest_IPUnsolicited ? 0 : 1);
	}

	void NNQueryTask::PerformSubmit(NNBackendRequest task_params) {
		Redis::Response reply;

		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		NN::ConnectionSummary summary = task_params.summary;

		OS::Address address = task_params.peer->getAddress();
		OS::Address private_address = summary.private_address;


		int client_index = summary.index;
		int num_addresses = 0;

		std::string nn_key;
		std::ostringstream nn_key_ss;
		nn_key_ss << "nn_client_" << client_index << "_" << task_params.peer->GetCookie();
		nn_key = nn_key_ss.str();

		Redis::Command(mp_redis_connection, 0, "HSET %s usegameport %d", nn_key.c_str(), summary.usegameport);
		Redis::Command(mp_redis_connection, 0, "HSET %s cookie %d", nn_key.c_str(), summary.cookie);
		Redis::Command(mp_redis_connection, 0, "HSET %s clientindex %d", nn_key.c_str(), client_index);

		reply = Redis::Command(mp_redis_connection, 0, "HINCRBY %s address_hits 1", nn_key.c_str());
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			num_addresses = reply.values[0].value._int;
		}

		Redis::Command(mp_redis_connection, 0, "HSET %s address_%d \"%s\"", nn_key.c_str(), summary.port_type, address.ToString().c_str());

		if (private_address.GetPort() != 0) {
			Redis::Command(mp_redis_connection, 0, "HSET %s private_address \"%s\"", nn_key.c_str(), private_address.ToString().c_str());
			if (task_params.peer->getUseGamePort()) {
				Redis::Command(mp_redis_connection, 0, "HSET %s gameport \"%d\"", nn_key.c_str(), private_address.GetPort());
			}
		}

		if (num_addresses >= task_params.peer->NumRequiredAddresses()) {
			Redis::Command(mp_redis_connection, 0, "PUBLISH %s '\\msg\\final_peer\\cookie\\%d\\client_index\\%d'", nn_channel, summary.cookie, client_index);
		}
	}
	ConnectionSummary NNQueryTask::LoadConnectionSummary(std::string redis_key) {
		ConnectionSummary connection_summary;
		Redis::Command(mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		Redis::Response reply;

		reply = Redis::Command(mp_redis_connection, 0, "EXISTS %s", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return connection_summary;

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s usegameport", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.usegameport = atoi((reply.values[0].value._str).c_str());

		if (connection_summary.usegameport) {
			reply = Redis::Command(mp_redis_connection, 0, "HGET %s gameport", redis_key.c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				goto error_cleanup;

			if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
				connection_summary.gameport = atoi((reply.values[0].value._str).c_str());
		}

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s clientindex", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.index = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s cookie", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.cookie = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s private_address", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		connection_summary.private_address = OS::Address(reply.values.front().value._str.c_str());


		int address_counter = 0;
		if (!connection_summary.usegameport) {
			address_counter = 1;
		}
		while (true) {
			std::ostringstream key_ss;
			key_ss << "address_" << address_counter;
			reply = Redis::Command(mp_redis_connection, 0, "HEXISTS %s %s", redis_key.c_str(), key_ss.str().c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				break;
			if (reply.values[0].value._int == 0)
				break;
			
			
			reply = Redis::Command(mp_redis_connection, 0, "HGET %s %s", redis_key.c_str(), key_ss.str().c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				goto error_cleanup;

			connection_summary.m_port_type_addresses[address_counter++] = OS::Address(reply.values.front().value._str.c_str());
		}

	error_cleanup:
		return connection_summary;
	}

	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 1;
		mp_redis_async_connection = Redis::Connect(OS::g_redisAddress, t);
		mp_async_lookup_task = new NNQueryTask(NUM_NN_QUERY_THREADS+1);
		Redis::LoopingCommand(mp_redis_async_connection, 0, NNQueryTask::onRedisMessage, thread->getParams(), "SUBSCRIBE %s", nn_channel);
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
		delete m_task_pool;

		mp_async_thread->SignalExit(true);
		delete mp_async_thread;
		Redis::Disconnect(mp_redis_async_retrival_connection);
	}

}
