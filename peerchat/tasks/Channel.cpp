#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    const char *mp_pk_channel_name = "PEERCHATCHANID";
	int GetPeerchatChannelID(TaskThreadData *thread_data) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_channel_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}

    ChannelSummary LookupChannelById(TaskThreadData *thread_data, int channel_id) {
        ChannelSummary summary;
        Redis::Response reply;
        reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d name", channel_id);
        if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
            goto error_end;
        }
        summary.channel_name = reply.values[0].value._str;
        summary.channel_id = channel_id;

        summary.users = GetChannelUsers(thread_data, channel_id);

        error_end:
            return summary;
    }


    ChannelSummary CreateChannel(TaskThreadData *thread_data, std::string name) {
        int channel_id = GetPeerchatChannelID(thread_data);

        ChannelSummary summary;

        summary.channel_id = channel_id;
        summary.channel_name = name;
        struct timeval curtime;
        gettimeofday(&curtime, NULL);
        summary.created_at = curtime;

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d name %s", channel_id, name.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d %d", channel_id, CHANNEL_EXPIRE_TIME);

        Redis::Command(thread_data->mp_redis_connection, 0, "SET channelname_%s %d", name.c_str(), channel_id);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channelname_%s %d", name.c_str(), CHANNEL_EXPIRE_TIME);
        return summary;        
    }

    int GetChannelIdByName(TaskThreadData *thread_data, std::string name) {
		Redis::Value v;
        Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET channelname_%s", name.c_str());
        if (Redis::CheckError(reply))
            return 0;
        if(reply.values.size() < 1)
            return 0;
        v = reply.values[0];

        int id = 0;
        if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
            id = atoi(v.value._str.c_str());
        } else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
            id = v.value._int;
        }        
        return id;
    }
    ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name) {
        int id = GetChannelIdByName(thread_data, name);
        if(id != 0) {
            return LookupChannelById(thread_data, id);
        }
        //doesn't exist, create

        return CreateChannel(thread_data, name);
    }
    void AddUserToChannel(TaskThreadData *thread_data, int user_id, int channel_id) {
        Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY channel_%d_users 1 \"%d\"", channel_id, user_id);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d_users %d", channel_id, CHANNEL_EXPIRE_TIME);

        std::ostringstream message;
		message << "\\type\\join\\channel_id\\" << channel_id << "\\user_id\\" << user_id;
        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_channel_message_routingkey, message.str().c_str());
    }
    std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id) {
        std::vector<ChannelUserSummary> result;
        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;

        do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN channel_%d_users %d", channel_id, cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

		 	if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
		 		cursor = atoi(v.value._str.c_str());
		 	} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
		 		cursor = v.value._int;
		 	}
            for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
                ChannelUserSummary summary;
                std::string user_id = arr.arr_value.values[i].second.value._str;
                summary.channel_id = channel_id;
                summary.user_id = atoi(user_id.c_str());
                result.push_back(summary);
            }
        } while(cursor != 0);
        error_cleanup:
        return result;
    }
}