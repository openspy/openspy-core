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

        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", user_key.c_str(), USER_EXPIRE_TIME);
        void *reply;

        if(request.summary.nick.length() != 0) {
            std::string formatted_name_original;
            std::string original_nick = request.summary.nick;
            std::transform(original_nick.begin(),original_nick.end(),std::back_inserter(formatted_name_original),tolower);

            ss.str("");
            ss << "usernick_" << formatted_name_original;
            std::string usernick_key = ss.str();

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", usernick_key.c_str(), USER_EXPIRE_TIME);

            redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
            freeReplyObject(reply);
        }

        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        freeReplyObject(reply);

        response.error_details.response_code = TaskShared::WebErrorCode_Success;

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}