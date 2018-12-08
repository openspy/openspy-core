#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/Task/TaskScheduler.h>

#include <string>
#include <sstream>

#include <server/NNPeer.h>
#include <server/NATMapper.h>

#include <OS/MessageQueue/MQInterface.h>

#include "tasks.h"

class NNTaskThreadData {
    public:
        MQ::IMQInterface *mp_mqinterface;
        Redis::Connection *mp_redis_connection;
};
extern const char *nn_channel_exchange;
extern const char *nn_channel_routingkey;

namespace NN {
    bool PerformSubmit(NNRequestData request, TaskThreadData  *thread_data) {
		Redis::Response reply;

		Redis::Command(thread_data->mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_NatNeg);

		NN::ConnectionSummary summary = request.summary;

		OS::Address address = request.peer->getAddress();
		OS::Address private_address = summary.private_address;


		int client_index = summary.index;

		int num_addresses = 0;

		std::string nn_key;
		std::ostringstream nn_key_ss;
		nn_key_ss << "nn_client_" << client_index << "_" << request.peer->GetCookie();
		nn_key = nn_key_ss.str();

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s required_addresses %d", nn_key.c_str(), request.peer->NumRequiredAddresses());

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s usegameport %d", nn_key.c_str(), summary.usegameport);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s cookie %d", nn_key.c_str(), summary.cookie);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s clientindex %d", nn_key.c_str(), client_index);

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HINCRBY %s address_hits 1", nn_key.c_str());
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			num_addresses = reply.values[0].value._int;
		}

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s address_%d \"%s\"", nn_key.c_str(), summary.port_type, address.ToString().c_str());

		if (private_address.GetPort() != 0) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s private_address \"%s\"", nn_key.c_str(), private_address.ToString().c_str());
			if (request.peer->getUseGamePort()) {
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameport \"%d\"", nn_key.c_str(), private_address.GetPort());
			}
		}

		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %s %d", nn_key.c_str(), NN_REDIS_EXPIRE_TIME);

		if (num_addresses >= request.peer->NumRequiredAddresses()) {
			//check if other peer is ready too, otherwise wait for it

			int opposite_client_index = client_index == 0 ? 1 : 0; //invert client index
			nn_key_ss.str("");
			nn_key_ss << "nn_client_" << opposite_client_index  << "_" << request.peer->GetCookie();

			ConnectionSummary summary = LoadConnectionSummary(thread_data->mp_redis_connection, nn_key_ss.str());


			if (summary.address_hits >= summary.required_addresses) {
				std::ostringstream ss;
				ss << "\\msg\\final_peer\\cookie\\" << summary.cookie << "\\client_index\\" << client_index;
				thread_data->mp_mqconnection->sendMessage(nn_channel_exchange, nn_channel_routingkey, ss.str());
                //BroadcastMessage(ss.str());
				ss.str(""); ss << "\\msg\\final_peer\\cookie\\" << summary.cookie << "\\client_index\\" << opposite_client_index;
				thread_data->mp_mqconnection->sendMessage(nn_channel_exchange, nn_channel_routingkey, ss.str());
			}
		}
		return true;
	}
}