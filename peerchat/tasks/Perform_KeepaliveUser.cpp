#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_KeepaliveUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;
        response.summary = request.summary;
        
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE user_%d %d", request.summary.id, USER_EXPIRE_TIME);

            if(request.summary.nick.length() != 0)
                Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE usernick_%s %d", request.summary.nick.c_str(), USER_EXPIRE_TIME);

        response.error_details.response_code = TaskShared::WebErrorCode_Success;

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}