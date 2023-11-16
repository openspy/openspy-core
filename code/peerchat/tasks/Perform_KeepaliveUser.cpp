#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_KeepaliveUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;
        response.summary = request.summary;

        std::ostringstream ss;
        ss << "user_" << request.summary.id;
        std::string user_key = ss.str();

        int num_redis_cmds = 0;

        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", user_key.c_str(), USER_EXPIRE_TIME); num_redis_cmds++;
        void *reply;

        if(request.summary.nick.length() != 0) {
            std::string formatted_name_original;
            std::string original_nick = request.summary.nick;
            std::transform(original_nick.begin(),original_nick.end(),std::back_inserter(formatted_name_original),tolower);

            ss.str("");
            ss << "usernick_" << formatted_name_original;
            std::string usernick_key = ss.str();

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", usernick_key.c_str(), USER_EXPIRE_TIME); num_redis_cmds++;

        }


        std::string user_channels_key;
        ss.str("");
        ss << "user_" << request.summary.id << "_channels";
        user_channels_key = ss.str();
        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", user_channels_key.c_str(), USER_EXPIRE_TIME); num_redis_cmds++;


        //refresh user and associated channels expire time
        std::map<int, int>::iterator it = request.channel_flags.begin();
        while(it != request.channel_flags.end()) {
            std::pair<int, int> p = *it;

            std::string redis_key;
            ss.str("");
            ss << "channel_" << p.first << "_user_" << request.summary.id;
            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", redis_key.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s_custkeys %d", redis_key.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;

            ss.str("");
            ss << "channel_" << p.first;
            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", redis_key.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;

            ss.str("");
            ss << "channel_" << p.first << "_users";
            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", redis_key.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;

            ss.str("");
            ss << "channel_" << p.first << "_usermodes";
            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", redis_key.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;
            
            it++;
        }


        for (int i = 0; i < num_redis_cmds; i++) {
            redisGetReply(thread_data->mp_redis_connection, (void**)&reply);
            freeReplyObject(reply);
        }

        response.error_details.response_code = TaskShared::WebErrorCode_Success;

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}