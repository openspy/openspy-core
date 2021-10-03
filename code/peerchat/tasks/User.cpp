#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    UserSummary GetUserSummaryByName(TaskThreadData *thread_data, std::string name) {
		
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		std::string formatted_name;
		std::transform(name.begin(),name.end(),std::back_inserter(formatted_name),tolower);

        Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS usernick_%s", formatted_name.c_str());
        Redis::Value v;

        int id = 0;
        if (Redis::CheckError(reply) || reply.values.size() == 0) {
            goto error_end;
        } else {
            v = reply.values[0];
            if (!((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0))) {
                goto error_end;
            }
        }

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET usernick_%s", formatted_name.c_str());
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        
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
		Redis::Value v, arr;


        UserSummary summary;
        if (user_id == -1) {
            return *server_userSummary;
        }

        Redis::Response reply;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);


        reply = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS user_%d", user_id);

        bool user_exists = false;
        if (!(Redis::CheckError(reply) || reply.values.size() == 0)) {
            v = reply.values.front();
            if ((v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) || (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0)) {
                user_exists = true;
            }
        }

        if(!user_exists) {
            summary.id = 0;
            return summary;
        }


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

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d modeflags", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.modeflags = atoi(reply.values[0].value._str.c_str());

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d operflags", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.operflags = atoi(reply.values[0].value._str.c_str());

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d profileid", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.profileid = atoi(reply.values[0].value._str.c_str());

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d userid", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.userid = atoi(reply.values[0].value._str.c_str());

        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET user_%d gameid", user_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.gameid = atoi(reply.values[0].value._str.c_str());

        summary.id = user_id;

        error_end:
            return summary;
    }
    int CountServerUsers(TaskThreadData* thread_data) {
        int count = 0;
        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;
		std::vector<ChannelSummary> channels;
		ChannelSummary summary;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH usernick_*", cursor);
			if (Redis::CheckError(reply))
				break;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {

				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.value._int;
				}
				count += arr.arr_value.values.size();
			}
			else break;
        } while(cursor != 0);
        return count;
    }
}