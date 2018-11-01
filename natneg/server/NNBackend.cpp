#include <OS/Config/AppConfig.h>
#include "NNPeer.h"
#include "NNBackend.h"
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>


#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <sstream>
#define NATNEG_COOKIE_TIME 30 
namespace NN {
	OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool = NULL;
	Redis::Connection *mp_redis_async_retrival_connection;
	OS::CThread *mp_async_thread;
	NNQueryTask *mp_async_lookup_task = NULL;

	amqp_socket_t *mp_rabbitmq_socket = NULL, *mp_rabbitmq_sender_socket = NULL;
	amqp_connection_state_t mp_rabbitmq_conn, mp_rabbitmq_sender_conn;

	const char *nn_channel_exchange = "amq.topic", *nn_channel_routingkey="natneg";
	const char *nn_channel_queuename = "natneg.backend";

	void *NNQueryTask::TaskThread(OS::CThread *thread) {
		NNQueryTask *task = (NNQueryTask *)thread->getParams();
		while (thread->isRunning() && (!task->m_request_list.empty() || task->mp_thread_poller->wait(NN_WAIT_MAX_TIME)) && thread->isRunning()) {
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
		std::ostringstream ss;
		ss << "\\msg\\natneg_erttest\\address\\" << address.ToString() << "\\type\\" << (task_params.type == ENNQueryRequestType_PerformERTTest_IPUnsolicited) ? 0 : 1;
		BroadcastMessage(ss.str());
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

		Redis::Command(mp_redis_connection, 0, "HSET %s required_addresses %d", nn_key.c_str(), task_params.peer->NumRequiredAddresses());

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

		Redis::Command(mp_redis_connection, 0, "EXPIRE %s %d", nn_key.c_str(), NN_REDIS_EXPIRE_TIME);

		if (num_addresses >= task_params.peer->NumRequiredAddresses()) {
			//check if other peer is ready too, otherwise wait for it

			int opposite_client_index = client_index == 0 ? 1 : 0; //invert client index
			nn_key_ss.str("");
			nn_key_ss << "nn_client_" << opposite_client_index  << "_" << task_params.peer->GetCookie();

			ConnectionSummary summary = LoadConnectionSummary(nn_key_ss.str());


			if (summary.address_hits >= summary.required_addresses) {
				std::ostringstream ss;
				ss << "\\msg\\final_peer\\cookie\\" << summary.cookie << "\\client_index\\" << client_index;
				BroadcastMessage(ss.str());
				ss.str(""); ss << "\\msg\\final_peer\\cookie\\" << summary.cookie << "\\client_index\\" << opposite_client_index;
				BroadcastMessage(ss.str());
			}
			
		}
	}
	void NNQueryTask::BroadcastMessage(std::string message) {
		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = 2; /* persistent delivery mode */
		amqp_basic_publish(mp_rabbitmq_sender_conn, 1, amqp_cstring_bytes(nn_channel_exchange),
			amqp_cstring_bytes(nn_channel_routingkey), 0, 0,
			&props, amqp_cstring_bytes(message.c_str()));
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

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s required_addresses", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.required_addresses = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(mp_redis_connection, 0, "HGET %s address_hits", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.address_hits = atoi((reply.values[0].value._str).c_str());


	error_cleanup:
		return connection_summary;
	}

	void *setup_redis_async(OS::CThread *thread) {
		mp_async_lookup_task = new NNQueryTask(NUM_NN_QUERY_THREADS+1);

		NN::Server *server = (NN::Server *)thread->getParams();

		for (;;) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;

			amqp_maybe_release_buffers(mp_rabbitmq_conn);

			res = amqp_consume_message(mp_rabbitmq_conn, &envelope, NULL, 0);
			

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			std::map<std::string, std::string> kv_data = OS::KeyStringToMap(message);
			if (kv_data.find("msg") != kv_data.end()) {
				std::string msg = kv_data["msg"];
				if (msg.compare("final_peer") == 0) {
					NNCookieType cookie = (NNCookieType)atoi(kv_data["cookie"].c_str());
					int client_idx = atoi(kv_data["client_index"].c_str());
					int opposite_client_idx = client_idx == 0 ? 1 : 0;
					std::vector<NN::Peer *> peer_list = server->FindConnections(cookie, opposite_client_idx, true);

					std::ostringstream nn_key_ss;
					nn_key_ss << "nn_client_" << client_idx << "_" << cookie;

					ConnectionSummary summary = mp_async_lookup_task->LoadConnectionSummary(nn_key_ss.str());
					NAT nat;
					OS::Address next_public_address, next_private_address;
					NN::LoadSummaryIntoNAT(summary, nat);
					NN::DetermineNatType(nat);
					NN::DetermineNextAddress(nat, next_public_address, next_private_address);

					std::vector<NN::Peer *>::iterator it = peer_list.begin();

					while (it != peer_list.end()) {
						NN::Peer *peer = *it;
						peer->OnGotPeerAddress(next_public_address, next_private_address, nat);
						peer->DecRef();
						it++;
					}
				}
			}

			amqp_basic_ack(mp_rabbitmq_conn, envelope.channel, envelope.delivery_tag, false);

			amqp_destroy_envelope(&envelope);
		}

		amqp_channel_close(mp_rabbitmq_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(mp_rabbitmq_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(mp_rabbitmq_conn);

		return NULL;
	}
	void SetupTaskPool(NN::Server* server) {

		std::string rabbitmq_address;
		int rabbitmq_port;
		OS::g_config->GetVariableString("", "rabbitmq_host", rabbitmq_address);
		OS::g_config->GetVariableInt("", "rabbitmq_port", rabbitmq_port);

		std::string rabbitmq_user, rabbitmq_pass;
		OS::g_config->GetVariableString("", "rabbitmq_user", rabbitmq_user);
		OS::g_config->GetVariableString("", "rabbitmq_password", rabbitmq_pass);


		std::string rabbitmq_vhost;
		OS::g_config->GetVariableString("", "rabbitmq_vhost", rabbitmq_vhost);


		//init listener
		mp_rabbitmq_conn = amqp_new_connection();
		mp_rabbitmq_socket = amqp_tcp_socket_new(mp_rabbitmq_conn);
		int status = amqp_socket_open(mp_rabbitmq_socket, rabbitmq_address.c_str(), rabbitmq_port);
		amqp_login(mp_rabbitmq_conn, rabbitmq_vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
			rabbitmq_user, rabbitmq_pass);
		amqp_channel_open(mp_rabbitmq_conn, 1);
		amqp_get_rpc_reply(mp_rabbitmq_conn); //TODO: channel error check

		amqp_queue_bind(mp_rabbitmq_conn, 1, amqp_cstring_bytes(nn_channel_queuename), amqp_cstring_bytes(nn_channel_exchange),
			amqp_cstring_bytes(nn_channel_routingkey), amqp_empty_table);

		amqp_get_rpc_reply(mp_rabbitmq_conn);

		amqp_basic_consume(mp_rabbitmq_conn, 1, amqp_cstring_bytes(nn_channel_queuename), amqp_empty_bytes, 0, 1, 0,
			amqp_empty_table);

		amqp_get_rpc_reply(mp_rabbitmq_conn);

		//init sender
		mp_rabbitmq_sender_conn = amqp_new_connection();
		mp_rabbitmq_sender_socket = amqp_tcp_socket_new(mp_rabbitmq_sender_conn);

		status = amqp_socket_open(mp_rabbitmq_sender_socket, rabbitmq_address.c_str(), rabbitmq_port);
		amqp_login(mp_rabbitmq_sender_conn, rabbitmq_vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
			rabbitmq_user, rabbitmq_pass);
		amqp_channel_open(mp_rabbitmq_sender_conn, 1);
		amqp_get_rpc_reply(mp_rabbitmq_sender_conn); //TODO: channel error check

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
