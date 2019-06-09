#include "tasks.h"
#define USER_EXPIRE_TIME 300
namespace Peerchat {
    const char *mp_pk_name = "PEERCHATID";
	int GetPeerchatUserID(TaskThreadData *thread_data) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
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
    bool Perform_SetUserDetails(PeerchatBackendRequest request, TaskThreadData *thread_data) {

        TaskResponse response;
        response.summary = request.summary;
        response.summary.id = GetPeerchatUserID(thread_data);

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET user_%d username %s", response.summary.id, request.summary.username.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET user_%d nick %s", response.summary.id, request.summary.nick.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET user_%d realname %s", response.summary.id, request.summary.realname.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET user_%d hostname %s", response.summary.id, request.summary.hostname.c_str());

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET usernick_%s %d", request.summary.username.c_str(), response.summary.id);

        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE user_%d %d", response.summary.id, USER_EXPIRE_TIME);

        response.error_details.response_code = TaskShared::WebErrorCode_Success;
        
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}