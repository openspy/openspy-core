#ifndef _NN_BACKEND_H
#define _NN_BACKEND_H

#include "NNDriver.h"
#include "NNPeer.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <OS/socketlib/socketlib.h>

#include <vector>
#include <map>
#include <string>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/adapters/libevent.h>
#undef _WINSOCK2API_

namespace NN {
	struct _NNBackendRequest;
	typedef struct {
		uint32_t cookie;
		uint8_t client_index;
		OS::Address address;
	} NNClientData;
	typedef void (*mpNNQueryCB)(NNClientData *data, void *extra);
	enum ENNQueryRequestType {
		ENNQueryRequestType_SubmitCookie,
	};
	typedef struct _NNBackendRequest {
		ENNQueryRequestType type;

		NNClientData data;

		mpNNQueryCB callback;
		void *extra;
	} NNBackendRequest;

	class NNQueryTask : public OS::Task<NNBackendRequest> {
		public:
			NNQueryTask();
			~NNQueryTask();
			static NNQueryTask *getQueryTask();


			void AddDriver(NN::Driver *driver);
			void RemoveDriver(NN::Driver *driver);

			static void SubmitClient(Peer *peer);

		private:
			static void *TaskThread(OS::CThread *thread);

			static void *setup_redis_async(OS::CThread *thread);

			static void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata);

			void PerformSubmit(NNBackendRequest request);

			std::vector<NN::Driver *> m_drivers;
			redisContext *mp_redis_connection;
			redisContext *mp_redis_async_retrival_connection;
			redisAsyncContext *mp_redis_async_connection;

			static NNQueryTask *m_task_singleton;
	};
}
#endif //_NN_BACKEND_H