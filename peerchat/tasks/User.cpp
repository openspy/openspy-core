#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    UserSummary LookupUserById(TaskThreadData *thread_data, int user_id) {
        UserSummary summary;
        Redis::Response reply;
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