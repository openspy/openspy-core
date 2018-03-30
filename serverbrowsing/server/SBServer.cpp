#include "SBPeer.h"
#include "SBServer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>

SBServer::SBServer() : INetServer() {
	gettimeofday(&m_last_analytics_submit_time, NULL);
}
SBServer::~SBServer() {
	MM::ShutdownTaskPool();
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		delete *it;
		it++;
	}
}
void SBServer::init() {
	MM::SetupTaskPool(this);
}
void SBServer::tick() {
	struct timeval current_time;
	gettimeofday(&current_time, NULL);
	if(current_time.tv_sec - m_last_analytics_submit_time.tv_sec > ANALYTICS_SUBMIT_TIME) {
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
void SBServer::SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool) {
	mp_task_pool = pool;
	const std::vector<MM::MMQueryTask *> task_list = pool->getTasks();
	std::vector<MM::MMQueryTask *>::const_iterator it = task_list.begin();
	while (it != task_list.end()) {
		MM::MMQueryTask *task = *it;
		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		while (it2 != m_net_drivers.end()) {
			SB::Driver *driver = (SB::Driver *)*it2;
			task->AddDriver(driver);
			it2++;
		}
		it++;
	}
}
OS::MetricInstance SBServer::GetMetrics() {
	OS::MetricInstance peer_metric;
	OS::MetricValue value, arr_value, arr_value2, container_val;
	
	std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
	int idx = 0;
	while (it2 != m_net_drivers.end()) {
		SB::Driver *driver = (SB::Driver *)*it2;
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

void SBServer::debug_dump() {
	std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
	while (it2 != m_net_drivers.end()) {
		SB::Driver *driver = (SB::Driver *)*it2;
		driver->debug_dump();
		it2++;
	}

	printf("Task Pool: \n");
	std::vector<MM::MMQueryTask *> task_list = mp_task_pool->getTasks();
	std::vector<MM::MMQueryTask *>::const_iterator it = task_list.begin();
	while (it != task_list.end()) {
		MM::MMQueryTask *task = *it;
		task->debug_dump();
		it++;
	}
}