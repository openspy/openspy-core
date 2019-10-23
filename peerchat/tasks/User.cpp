#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    UserSummary GetUserSummaryByName(TaskThreadData *thread_data, std::string name) {
		
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

        Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS usernick_%s", name.c_str());
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

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET usernick_%s", name.c_str());
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        int id = 0;
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			id = atoi(reply.values[0].value._str.c_str());
		}
		else if(reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			id = reply.values[0].value._int;
		}
		return LookupUserById(thread_data, id);
	error_end:
		return UserSummary();
    }
    UserSummary LookupUserById(TaskThreadData *thread_data, int user_id) {
        UserSummary summary;
        Redis::Response reply;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d username", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.username = reply.values[0].value._str;

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d nick", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.nick = reply.values[0].value._str;


        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d realname", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.realname = reply.values[0].value._str;

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d hostname", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.hostname = reply.values[0].value._str;

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d address", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.address = reply.values[0].value._str;

        summary.id = user_id;

        error_end:
            return summary;
    }
}