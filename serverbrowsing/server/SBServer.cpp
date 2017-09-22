#include "SBPeer.h"
#include "SBServer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>

SBServer::SBServer() : INetServer() {
	gettimeofday(&m_last_analytics_submit_time, NULL);
}
SBServer::~SBServer() {
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

	
	//NetworkTick(); //called by the main loop
}
void SBServer::shutdown() {

}
void SBServer::SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool) {
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
	OS::MetricValue value, arr_value;
	
	value.value._str = "127.0.0.1:555";
	value.key = "ip";
	value.type = OS::MetricType_String;
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.type = OS::MetricType_Integer;
	value.value._int = 11662;
	value.key = "bytes_in";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.value._int = 512312461;
	value.key = "bytes_out";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.value._int = 521222;
	value.key = "packets_in";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.value._int = 99915256;
	value.key = "packets_out";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.value._int = 99915256;
	value.key = "packets_out";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

	value.value._int = 666;
	value.key = "total_requets";
	arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));
	
	arr_value.type = OS::MetricType_Array;
	peer_metric.key = "peer";
	peer_metric.value = arr_value;
	return peer_metric;
}