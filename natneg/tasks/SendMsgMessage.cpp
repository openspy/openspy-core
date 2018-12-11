#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include <server/NNServer.h>
#include <server/NNPeer.h>
#include <server/NATMapper.h>
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>

namespace NN {
    bool Handle_SendMsg(TaskThreadData  *thread_data, std::string message) {
			NN::Server *server = (NN::Server *)thread_data->server;

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

					ConnectionSummary summary = LoadConnectionSummary(thread_data->mp_redis_connection, nn_key_ss.str());
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
			return true;
    }
}