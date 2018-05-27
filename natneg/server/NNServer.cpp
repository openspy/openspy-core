#include "NNServer.h"
#include "NNPeer.h"
#include "NNDriver.h"
#include "NNBackend.h"
#include <iterator>
#include <OS/Analytics/AnalyticsMgr.h>
namespace NN {

	Server::Server() : INetServer() {
		gettimeofday(&m_last_analytics_submit_time, NULL);
	}
	void Server::init() {
		NN::SetupTaskPool(this);
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

	}
	void Server::SetTaskPool(OS::TaskPool<NN::NNQueryTask, NN::NNBackendRequest> *pool) {
		const std::vector<NN::NNQueryTask *> task_list = pool->getTasks();
		std::vector<NN::NNQueryTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			NN::NNQueryTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				NN::Driver *driver = (NN::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
	}

	std::vector<NN::Peer *> Server::FindConnections(NNCookieType cookie, int client_idx, bool inc_ref) {
		std::vector<NN::Peer *> peers;
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		NN::Peer *peer;
		while (it != m_net_drivers.end()) {
			NN::Driver *driver = (NN::Driver *)*it;
			std::vector<NN::Peer *> driver_peers = driver->find_clients(cookie, client_idx, inc_ref);
			peers.insert(peers.end(), driver_peers.begin(), driver_peers.end());
			it++;
		}
		return peers;
	}


	OS::MetricInstance Server::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue value, arr_value, arr_value2, container_val;

		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		int idx = 0;
		while (it2 != m_net_drivers.end()) {
			NN::Driver *driver = (NN::Driver *)*it2;
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