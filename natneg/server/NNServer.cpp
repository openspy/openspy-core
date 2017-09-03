#include "NNPeer.h"
#include "NNServer.h"
#include "NNDriver.h"
#include "NNBackend.h"
namespace NN {

	Server::Server() : INetServer() {
		
	}
	void Server::init() {
		NN::SetupTaskPool(this);
	}
	void Server::tick() {
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
	void Server::OnGotCookie(int cookie, int client_idx, OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			NN::Driver *driver = (NN::Driver *)*it;
			driver->OnGotCookie(cookie, client_idx, address);
			it++;
		}
	}
}