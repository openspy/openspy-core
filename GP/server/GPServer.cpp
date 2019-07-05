#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>
#include "GPPeer.h"
#include "GPServer.h"
#include "GPDriver.h"
namespace GP {
	Server::Server() : INetServer() {
	}
	void Server::init() {
		mp_auth_tasks = TaskShared::InitAuthTasks(this);
		mp_user_tasks = TaskShared::InitUserTasks(this);
		mp_profile_tasks = TaskShared::InitProfileTasks(this);
		mp_cdkey_tasks = TaskShared::InitCDKeyTasks(this);
		mp_gp_tasks = GP::InitTasks(this);
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