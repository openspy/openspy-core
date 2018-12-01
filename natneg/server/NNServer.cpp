#include "NNServer.h"
#include "NNPeer.h"
#include "NNDriver.h"
#include <tasks/tasks.h>
#include <iterator>
namespace NN {

	Server::Server() : INetServer() {
		mp_task_scheduler = NULL;
	}
	void Server::init() {
		mp_task_scheduler = InitTasks();
	}
	void Server::tick() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think(false);
			it++;
		}
		NetworkTick();
	}
	void Server::shutdown() {

	}

	std::vector<NN::Peer *> Server::FindConnections(NNCookieType cookie, int client_idx, bool inc_ref) {
		std::vector<NN::Peer *> peers;
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			NN::Driver *driver = (NN::Driver *)*it;
			std::vector<NN::Peer *> driver_peers = driver->find_clients(cookie, client_idx, inc_ref);
			peers.insert(peers.end(), driver_peers.begin(), driver_peers.end());
			it++;
		}
		return peers;
	}
}