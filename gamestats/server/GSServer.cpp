#include <OS/OpenSpy.h>
#include <OS/TaskPool.h>
#include "GSPeer.h"
#include "GSServer.h"
#include "GSDriver.h"

namespace GS {
	Server::Server() : INetServer(){
		gettimeofday(&m_last_analytics_submit_time, NULL);
	}
	void Server::init() {
		GSBackend::SetupTaskPool(this);
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}
	void Server::SetTaskPool(OS::TaskPool<GSBackend::PersistBackendTask, GSBackend::PersistBackendRequest> *pool) {
		const std::vector<GSBackend::PersistBackendTask *> task_list = pool->getTasks();
		std::vector<GSBackend::PersistBackendTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			GSBackend::PersistBackendTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				GS::Driver *driver = (GS::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
	}
	OS::MetricInstance Server::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue value, arr_value, arr_value2, container_val;

		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		int idx = 0;
		while (it2 != m_net_drivers.end()) {
			GS::Driver *driver = (GS::Driver *)*it2;
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