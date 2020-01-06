#include "tasks.h"
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
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        TaskResponse response;
        response.summary = request.summary;

        response.profile.uniquenick = request.summary.nick;

        UserSummary userDetails = request.peer->GetUserDetails();
        bool nick_update = false;
        if(userDetails.id != 0) {
            response.summary.id = userDetails.id;
        } else {
            response.summary.id = GetPeerchatUserID(thread_data);
            nick_update = true;
        }

        Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS usernick_%s", request.summary.nick.c_str());
        Redis::Value v;

        bool nick_exists = false;
        if (Redis::CheckError(reply) || reply.values.size() == 0) {
            nick_exists = true;
        } else {
            v = reply.values[0];
            if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0)) {
                nick_exists = true;
            }
        }

        if(request.summary.nick.compare(userDetails.nick) == 0) {
            nick_exists = false;
		}
		else {
			nick_update = true;
		}

		request.summary.address = request.peer->getAddress();
		response.summary.address = request.summary.address;
        if(!nick_exists) {
            response.summary.nick = request.summary.nick;
            if(nick_update) {
                Redis::Command(thread_data->mp_redis_connection, 0, "DEL usernick_%s", userDetails.nick.c_str());

				Redis::Command(thread_data->mp_redis_connection, 0, "HSET user_%d nick %s", response.summary.id, request.summary.nick.c_str());

                Redis::Command(thread_data->mp_redis_connection, 0, "SET usernick_%s %d", request.summary.nick.c_str(), response.summary.id);
            } else {
                response.summary.nick = userDetails.nick;

                std::ostringstream ss;
                ss << "user_" << response.summary.id;
                request.summary.modeflags = 0;
                request.summary.gameid = 0;
                ApplyUserKeys(thread_data, ss.str(), request.summary, "", true);
                ApplyUserKeys(thread_data, ss.str(), request.summary, "custkey_");

                if(request.summary.nick.length() != 0) {
                    Redis::Command(thread_data->mp_redis_connection, 0, "DEL usernick_%s", userDetails.nick.c_str());
                    Redis::Command(thread_data->mp_redis_connection, 0, "SET usernick_%s %d", request.summary.nick.c_str(), response.summary.id);
                }
            }

            Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE user_%d %d", response.summary.id, USER_EXPIRE_TIME);

            if(request.summary.nick.length() != 0)
                Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE usernick_%s %d", request.summary.nick.c_str(), USER_EXPIRE_TIME);

            response.error_details.response_code = TaskShared::WebErrorCode_Success;
        } else {
            response.error_details.response_code = TaskShared::WebErrorCode_UniqueNickInUse;
        }
        
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}