#ifndef _NN_BACKEND_H
#define _NN_BACKEND_H

#include "NNServer.h"
#include "NNDriver.h"
#include "NNPeer.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <OS/socketlib/socketlib.h>
#include <OS/Timer/HiResTimer.h>

#include <vector>
#include <map>
#include <string>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

namespace NN {
	struct _NNBackendRequest;
	enum ENNQueryRequestType {
		ENNQueryRequestType_SubmitClient,
		ENNQueryRequestType_PerformERTTest,
	};
	typedef struct _NNBackendRequest {
		ENNQueryRequestType type;
		void *extra;
		NN::Peer *peer;
	} NNBackendRequest;

	class NNQueryTask : public OS::Task<NNBackendRequest> {
		public:
			NNQueryTask();
			~NNQueryTask();
			static void Shutdown();
			void AddDriver(NN::Driver *driver);
			void RemoveDriver(NN::Driver *driver);

		private:
			static void *TaskThread(OS::CThread *thread);

			static void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);

			void PerformSubmit(NNBackendRequest request);
			void PerformERTTest(NNBackendRequest request);
			std::vector<NN::Driver *> m_drivers;
			Redis::Connection *mp_redis_connection;
			time_t m_redis_timeout;
			bool m_thread_awake;
			OS::HiResTimer *mp_timer;
	};
	#define NUM_NN_QUERY_THREADS 8
	extern OS::TaskPool<NNQueryTask, NNBackendRequest> *m_task_pool;
	void SetupTaskPool(NN::Server *server);
	void *setup_redis_async(OS::CThread *thread);
}
#endif //_NN_BACKEND_H