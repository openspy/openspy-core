#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"

namespace FESL {
	Server::Server() : INetServer() {
		mp_auth_tasks = TaskShared::InitAuthTasks(this);
		mp_user_tasks = TaskShared::InitUserTasks(this);
		mp_profile_tasks = TaskShared::InitProfileTasks(this);
	}
	void Server::init() {
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
	void Server::OnUserAuth(std::string session_key, int userid, int profileid) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			FESL::Driver *driver = (FESL::Driver *) *it;
			driver->OnUserAuth(session_key, userid, profileid);
			it++;
		}
	}
}