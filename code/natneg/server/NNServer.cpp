#include "NNServer.h"
#include "NNDriver.h"
#include <tasks/tasks.h>
#include <iterator>
namespace NN {

	Server::Server() : INetServer() {
		uv_loop_set_data(uv_default_loop(), this);
		InitTasks();
	}
	Server::~Server() {
	}
	void Server::tick() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think();
			it++;
		}
		NetworkTick();
	}
	NN::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			if(OS::Address(driver->GetAddress()) == address) {
				return (NN::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}
}