#ifndef _NN_BACKEND_H
#define _NN_BACKEND_H

#include "NNServer.h"
#include "NNDriver.h"
#include "NNPeer.h"
#include "NATMapper.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <OS/Timer/HiResTimer.h>

#include <vector>
#include <map>
#include <string>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#define NN_REDIS_EXPIRE_TIME 120

namespace NN {
	enum ENNQueryRequestType {
		ENNQueryRequestType_SubmitClient,
		ENNQueryRequestType_PerformERTTest_IPUnsolicited,
		ENNQueryRequestType_PerformERTTest_IPPortUnsolicited,
	};
	class NNBackendRequest {
		public:
			NNBackendRequest() {
				peer = NULL;
				extra = NULL;
				type = ENNQueryRequestType_SubmitClient;
			}
			~NNBackendRequest() { }
			ENNQueryRequestType type;
			NN::ConnectionSummary summary;
			void *extra;
			NN::Peer *peer;
	};

	class NNQueryTask : public OS::Task<NNBackendRequest> {
		public:
			NNQueryTask(int thread_id);
			~NNQueryTask();
			static void Shutdown();
			void AddDriver(NN::Driver *driver);
			void RemoveDriver(NN::Driver *driver);
			ConnectionSummary LoadConnectionSummary(std::string redis_key);
			void BroadcastMessage(std::string message);
		private:
			static void *TaskThread(OS::CThread *thread);


			void PerformSubmit(NNBackendRequest request);
			void PerformERTTest(NNBackendRequest request);
			std::vector<NN::Driver *> m_drivers;
			Redis::Connection *mp_redis_connection;
			time_t m_redis_timeout;
			bool m_thread_awake;
			OS::HiResTimer *mp_timer;
			int m_thread_id;
	};
	#define NUM_NN_QUERY_THREADS 8
	#define NN_WAIT_MAX_TIME 1500
	extern OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool;
	void SetupTaskPool(NN::Server *server);
	void *setup_redis_async(OS::CThread *thread);
}
#endif //_NN_BACKEND_H