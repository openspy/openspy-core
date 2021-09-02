#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
	int GetServerID(TaskThreadData *thread_data) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}
    bool PerformAllocateServerId(UTMasterRequest request, TaskThreadData *thread_data) {
        MMTaskResponse response;
        response.peer = request.peer;

        if(request.peer->GetServerID() != 0) {
            response.server_id = request.peer->GetServerID();
        } else {
            response.server_id = GetServerID(thread_data);
        }
        

        if(request.peer) {
            request.peer->DecRef();
        }
        if(request.callback) {
            request.callback(response);
        }
        return true;
    }
}