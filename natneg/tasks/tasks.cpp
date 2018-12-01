#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include "../server/NNPeer.h"
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>
#define NATNEG_COOKIE_TIME 30 


namespace NN {
    const char *nn_channel_exchange = "openspy.natneg", *nn_channel_routingkey="natneg.core";

    /*NNThreadData *ThreadDataFactory(TaskScheduler<NNRequestData, NNThreadData> *scheduler, EThreadInitState state, NNThreadData *data) {
		if(state == EThreadInitState_AllocThreadData) {
			data = new NNThreadData();
			std::string rabbitmq_address;
			OS::g_config->GetVariableString("", "rabbitmq_address", rabbitmq_address);

			std::string rabbitmq_user, rabbitmq_pass;
			OS::g_config->GetVariableString("", "rabbitmq_user", rabbitmq_user);
			OS::g_config->GetVariableString("", "rabbitmq_password", rabbitmq_pass);


			std::string rabbitmq_vhost;
			OS::g_config->GetVariableString("", "rabbitmq_vhost", rabbitmq_vhost);


			data->mp_mqconnection = (MQ::IMQInterface*)new MQ::rmqConnection(OS::Address(rabbitmq_address), rabbitmq_user, rabbitmq_pass, rabbitmq_vhost);

			struct timeval t;
			t.tv_usec = 0;
			t.tv_sec = 60;

			data->mp_redis_connection = Redis::Connect(OS::g_redisAddress, t);

			data->server = NULL;
		} else if(state == EThreadInitState_InitThreadData) {
			data->mp_mqconnection->declareReady();
		} else if(state == EThreadInitState_DeallocThreadData) {
			delete data;
			data = NULL;
		}
        return data;
    }*/
    TaskScheduler<NNRequestData, TaskThreadData> *InitTasks() {
        TaskScheduler<NNRequestData, TaskThreadData> *scheduler = new TaskScheduler<NNRequestData, TaskThreadData>(4);

        scheduler->SetThreadDataFactory(TaskScheduler<NNRequestData, TaskThreadData>::DefaultThreadDataFactory);

        scheduler->AddRequestHandler(ENNRequestType_SubmitClient, PerformSubmit);
        scheduler->AddRequestHandler(ENNRequestType_PerformERTTest_IPUnsolicited, PerformERTTest);
        scheduler->AddRequestHandler(ENNRequestType_PerformERTTest_IPPortUnsolicited, PerformERTTest);
        scheduler->AddRequestListener(nn_channel_exchange, nn_channel_routingkey, "send_msg", Handle_SendMsg);

		scheduler->DeclareReady();

        return scheduler;
    }

	ConnectionSummary LoadConnectionSummary(Redis::Connection *redis_connection, std::string redis_key) {
		int address_counter = 0;
		ConnectionSummary connection_summary;
		Redis::Command(redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		Redis::Response reply;

		reply = Redis::Command(redis_connection, 0, "EXISTS %s", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return connection_summary;

		reply = Redis::Command(redis_connection, 0, "HGET %s usegameport", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.usegameport = atoi((reply.values[0].value._str).c_str());

		if (connection_summary.usegameport) {
			reply = Redis::Command(redis_connection, 0, "HGET %s gameport", redis_key.c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				goto error_cleanup;

			if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
				connection_summary.gameport = atoi((reply.values[0].value._str).c_str());
		}

		reply = Redis::Command(redis_connection, 0, "HGET %s clientindex", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.index = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(redis_connection, 0, "HGET %s cookie", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.cookie = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(redis_connection, 0, "HGET %s private_address", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		connection_summary.private_address = OS::Address(reply.values.front().value._str.c_str());

		//if (!connection_summary.usegameport) {
		//	address_counter = 1;
		//}
		while (true) {
			std::ostringstream key_ss;
			key_ss << "address_" << address_counter;
			reply = Redis::Command(redis_connection, 0, "HEXISTS %s %s", redis_key.c_str(), key_ss.str().c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				break;
			if (reply.values[0].value._int == 0)
				break;
			
			
			reply = Redis::Command(redis_connection, 0, "HGET %s %s", redis_key.c_str(), key_ss.str().c_str());
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
				goto error_cleanup;

			connection_summary.m_port_type_addresses[address_counter++] = OS::Address(reply.values.front().value._str.c_str());
		}

		reply = Redis::Command(redis_connection, 0, "HGET %s required_addresses", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.required_addresses = atoi((reply.values[0].value._str).c_str());

		reply = Redis::Command(redis_connection, 0, "HGET %s address_hits", redis_key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING)
			connection_summary.address_hits = atoi((reply.values[0].value._str).c_str());


	error_cleanup:
		return connection_summary;
	}
}