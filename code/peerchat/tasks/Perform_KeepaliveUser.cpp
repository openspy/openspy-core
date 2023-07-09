#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_KeepaliveUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;
        response.summary = request.summary;

        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE user_%d %d", request.summary.id, USER_EXPIRE_TIME);

        void *reply;

        if(request.summary.nick.length() != 0) {
            std::string formatted_name_original;
            std::string original_nick = request.summary.nick;
            std::transform(original_nick.begin(),original_nick.end(),std::back_inserter(formatted_name_original),tolower);

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE usernick_%s %d", formatted_name_original.c_str(), USER_EXPIRE_TIME);

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