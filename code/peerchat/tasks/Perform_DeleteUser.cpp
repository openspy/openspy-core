#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_DeleteUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;

        UserSummary userDetails = request.summary;

        std::string formatted_name;
		std::transform(userDetails.nick.begin(),userDetails.nick.end(),std::back_inserter(formatted_name),tolower);

        std::ostringstream ss;
        ss << "usernick_" << formatted_name;
        std::string formated_key = ss.str();

        ss.str("");
        ss << "user_" << userDetails.id;
        std::string user_key = ss.str();


        int num_redis_cmds = 0;

        redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", formated_key.c_str()); num_redis_cmds++;
        redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", user_key.c_str()); num_redis_cmds++;
        redisAppendCommand(thread_data->mp_redis_connection, "DEL %s_custkeys", user_key.c_str()); num_redis_cmds++;

        redisReply *reply;
        for(int i=0;i<num_redis_cmds;i++) {            
            int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
            if(r == REDIS_OK) {
                freeReplyObject(reply);
            }
        }
        

        std::string user_channels_key;
        ss.str("");
        ss << "user_" << userDetails.id << "_channels";
        user_channels_key = ss.str();
        

        int cursor = 0;
        do {
            reply = (redisReply*)redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d", user_channels_key.c_str(), cursor);

            if (reply == NULL)
                break;

            if (reply->element[0]->type == REDIS_REPLY_STRING) {
                cursor = atoi(reply->element[0]->str);
            }
            else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
                cursor = reply->element[0]->integer;
            }

            for (size_t i = 0; i < reply->element[1]->elements; i += 2) {
                if (reply->element[1]->element[i]->type == REDIS_REPLY_STRING) {
                    int channel_id = atoi(reply->element[1]->element[i]->str);
                    ChannelSummary channelSummary = LookupChannelById(thread_data, channel_id);
                    RemoveUserFromChannel(thread_data, userDetails, channelSummary, "NA", "NA", userDetails, true);
                }
            }
            freeReplyObject(reply);
        } while (cursor != 0);

        reply = (redisReply*)redisCommand(thread_data->mp_redis_connection, "DEL %s_channels", user_key.c_str());
        if(reply) {
            freeReplyObject(reply);
        }

        response.error_details.response_code = TaskShared::WebErrorCode_Success;
        response.summary = userDetails;

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}
