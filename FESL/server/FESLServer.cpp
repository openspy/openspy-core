#include "FESLPeer.h"
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLBackend.h"
#include <OS/Analytics/AnalyticsMgr.h>
namespace FESL {
	Server::Server() : INetServer(){
		gettimeofday(&m_last_analytics_submit_time, NULL);
	}
	void Server::init() {
		FESLBackend::SetupFESLBackend(this);
	}
	void Server::tick() {
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_analytics_submit_time.tv_sec > ANALYTICS_SUBMIT_TIME) {
			OS::AnalyticsManager::getSingleton()->SubmitServer(this);
			gettimeofday(&m_last_analytics_submit_time, NULL);
		}

		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think(false);
			it++;
		}
		NetworkTick();
	}
	void Server::shutdown() {
		FESLBackend::ShutdownFESLBackend();
	}
	void Server::OnUserAuth(OS::Address remote_address, int userid, int profileid) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			FESL::Driver *driver = (FESL::Driver *) *it;
			driver->OnUserAuth(remote_address, userid, profileid);
			it++;
		}
	}
	OS::MetricInstance Server::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue value, arr_value, arr_value2, container_val;

		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		int idx = 0;
		while (it2 != m_net_drivers.end()) {
			FESL::Driver *driver = (FESL::Driver *)*it2;
			arr_value2 = driver->GetMetrics().value;
			it2++;
			arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, arr_value2));
		}

		arr_value.type = OS::MetricType_Array;
		arr_value.key = std::string(OS::g_hostName) + std::string(":") + std::string(OS::g_appName);

		container_val.type = OS::MetricType_Array;
		container_val.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, arr_value));
		peer_metric.value = container_val;
		return peer_metric;
	}
}