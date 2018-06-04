#include "NNServer.h"
#include "NNPeer.h"
#include "NNDriver.h"
#include "NNBackend.h"
#include <iterator>
namespace NN {

	Server::Server() : INetServer() {
	}
	void Server::init() {
		NN::SetupTaskPool(this);
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
	void Server::SetTaskPool(OS::TaskPool<NN::NNQueryTask, NN::NNBackendRequest> *pool) {
		const std::vector<NN::NNQueryTask *> task_list = pool->getTasks();
		std::vector<NN::NNQueryTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			NN::NNQueryTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				NN::Driver *driver = (NN::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
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