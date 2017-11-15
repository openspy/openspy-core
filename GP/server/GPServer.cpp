#include "GPPeer.h"
#include "GPServer.h"
#include "GPDriver.h"
#include <OS/Analytics/AnalyticsMgr.h>
namespace GP {
	Server::Server() : INetServer() {
		gettimeofday(&m_last_analytics_submit_time, NULL);
	}
	void Server::init() {
		GPBackend::SetupTaskPool(this);
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
		GPBackend::ShutdownTaskPool();
	}
	void Server::SetTaskPool(OS::TaskPool<GPBackend::GPBackendRedisTask, GPBackend::GPBackendRedisRequest> *pool) {
		const std::vector<GPBackend::GPBackendRedisTask *> task_list = pool->getTasks();
		std::vector<GPBackend::GPBackendRedisTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			GPBackend::GPBackendRedisTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				GP::Driver *driver = (GP::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
	}
	INetPeer *Server::findPeerByProfile(int profile_id) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		INetPeer *ret;
		GP::Driver *driver;
		while (it != m_net_drivers.end()) {
			driver = (GP::Driver *)*it;
			ret = driver->FindPeerByProfileID(profile_id);
			if (ret) {
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
	OS::MetricInstance Server::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue value, arr_value, arr_value2, container_val;

		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		int idx = 0;
		while (it2 != m_net_drivers.end()) {
			GP::Driver *driver = (GP::Driver *)*it2;
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