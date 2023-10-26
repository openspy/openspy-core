#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_DeleteUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;

        UserSummary userDetails = request.peer->GetUserDetails();

        std::string formatted_name;
		std::transform(userDetails.nick.begin(),userDetails.nick.end(),std::back_inserter(formatted_name),tolower);

        std::ostringstream ss;
        ss << "usernick_" << formatted_name;
        std::string formated_key = ss.str();

        ss.str("");
        ss << "user_" << userDetails.id;
        std::string user_key = ss.str();

        redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", formated_key.c_str());
        redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", user_key.c_str());

        void *reply;
        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        freeReplyObject(reply);

        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        freeReplyObject(reply);

        std::vector<int> channels = request.peer->GetChannels();
        std::vector<int>::iterator it = channels.begin();
        while (it != channels.end()) {
            int channel_id = *it;
            ChannelSummary channelSummary = LookupChannelById(thread_data, channel_id);
            RemoveUserFromChannel(thread_data, userDetails, channelSummary, "NA", "NA", userDetails, true);
            it++;
        }

        response.error_details.response_code = TaskShared::WebErrorCode_Success;

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}