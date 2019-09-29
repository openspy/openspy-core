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

    bool Handle_ChannelMessage(TaskThreadData *thread_data, std::string message) {
        printf("CHANNEL MSG: %s\n", message.c_str());
        OS::KVReader kvReader(message);
        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        int channel_id = 0, user_id = 0;
        if(kvReader.GetValue("type").compare("join") == 0) {
            channel_id = kvReader.GetValueInt("channel_id");
            user_id = kvReader.GetValueInt("user_id");
            server->OnChannelMessage("JOIN", "CHC!CHC@*", "#test", "");
        }
        return true;
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

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET channelname_%s %d", name.c_str(), channel_id);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channelname_%s %d", name.c_str(), CHANNEL_EXPIRE_TIME);
        return summary;        
    }


    //MOVE THIS
    ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name) {
        //XXX: lookup channel

        //doesn't exist, create

        return CreateChannel(thread_data, name);
    }
    void AddUserToChannel(TaskThreadData *thread_data, int user_id, int channel_id) {
        Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY channel_%d_users 1 \"%d\"", channel_id, user_id);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d_users %d", channel_id, CHANNEL_EXPIRE_TIME);

        printf("user join channel\n");


        std::ostringstream message;
		message << "\\type\\join" << "\\channel_id\\" << channel_id << "\\user_id\\" << user_id;
        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_channel_message_routingkey, message.str().c_str());
    }
    //

    bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name);
        AddUserToChannel(thread_data, request.peer->GetBackendId(), channel.channel_id);
        return true;
    }
}