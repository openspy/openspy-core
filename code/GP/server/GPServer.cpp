#include <tasks/tasks.h>
#include "GPPeer.h"
#include "GPServer.h"
#include "GPDriver.h"
#include "tasks/tasks.h"
namespace GP {
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
			driver->think(false);
			it++;
		}
		NetworkTick();
	}
	void Server::shutdown() {
	}
	INetPeer *Server::findPeerByProfile(int profile_id, bool inc_ref) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		INetPeer *ret;
		GP::Driver *driver;
		while (it != m_net_drivers.end()) {
			driver = (GP::Driver *)*it;
			ret = driver->FindPeerByProfileID(profile_id);
			if (ret) {
				if (inc_ref) {
					ret->IncRef();
				}
				return ret;
			}
			it++;
		}
		return NULL;
	}
	void Server::InformStatusUpdate(int from_profileid, GPShared::GPStatus status) {
		GP::Driver *driver;
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			driver = (GP::Driver *)*it;
			driver->InformStatusUpdate(from_profileid, status);
			it++;
		}
	}
}