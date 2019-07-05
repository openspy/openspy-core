#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"

namespace FESL {
	Server::Server() : INetServer() {
		mp_auth_tasks = TaskShared::InitAuthTasks(this);
		mp_user_tasks = TaskShared::InitUserTasks(this);
		mp_profile_tasks = TaskShared::InitProfileTasks(this);
		mp_geo_tasks = TaskShared::InitGeoTasks(this);
	}
	void Server::init() {
		TaskShared::GeoRequest request;
		request.type = TaskShared::EGeoTaskType_GetCountries;
		request.extra = this;
		request.peer = NULL;
		request.callback = GetCountriesCallback;
		mp_geo_tasks->AddRequest(request.type, request);
	}
	void Server::GetCountriesCallback(TaskShared::GeoTaskData auth_data, void *extra, INetPeer *peer) {
		if (auth_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			Server *server = (Server *)extra;
			server->m_countries = auth_data.countries;
		}
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