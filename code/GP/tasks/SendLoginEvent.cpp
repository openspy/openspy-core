#include "tasks.h"
namespace GP {
    bool Perform_SendLoginEvent(GPBackendRedisRequest request, TaskThreadData *thread_data) {

		if (request.peer)
			request.peer->DecRef();
        return false;
    }
}