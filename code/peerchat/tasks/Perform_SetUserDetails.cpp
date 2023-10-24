#include "tasks.h"
namespace Peerchat {
    const char *mp_pk_name = "PEERCHATID";
	int GetPeerchatUserID(TaskThreadData *thread_data) {
        int ret;
        
        redisAppendCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
        redisAppendCommand(thread_data->mp_redis_connection, "INCR %s", mp_pk_name);

        redisReply *reply;
        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        freeReplyObject(reply);

        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);

        ret = reply->integer;

        freeReplyObject(reply);
		return ret;
	}
    bool Perform_SetUserDetails(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        redisReply *reply;

        redisAppendCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);
        freeReplyObject(reply);

        TaskResponse response;
        response.summary = request.summary;
        response.profile.uniquenick = request.summary.nick;

        std::string formatted_name;
		std::transform(request.summary.nick.begin(),request.summary.nick.end(),std::back_inserter(formatted_name),tolower);

        std::string formatted_name_original;
        std::string original_nick = request.peer->GetUserDetails().nick;
		std::transform(original_nick.begin(),original_nick.end(),std::back_inserter(formatted_name_original),tolower);

        UserSummary userDetails = request.peer->GetUserDetails();
        bool nick_update = false;
        if(userDetails.id != 0) {
            response.summary.id = userDetails.id;
        } else {
            response.summary.id = GetPeerchatUserID(thread_data);
            nick_update = true;
        }

        redisAppendCommand(thread_data->mp_redis_connection, "EXISTS usernick_%s", formatted_name.c_str());

        bool nick_exists = false;

        redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        nick_exists = reply->integer;
        freeReplyObject(reply);

        if(request.summary.nick.compare(userDetails.nick) == 0) {
            nick_exists = false;
		}
		else {
			nick_update = true;
		}

		request.summary.address = request.peer->getAddress();
		response.summary.address = request.summary.address;

        int cmd_count = 0;
        if(!nick_exists) {
            response.summary.nick = request.summary.nick;
            if(nick_update) {
                redisAppendCommand(thread_data->mp_redis_connection, "DEL usernick_%s", formatted_name_original.c_str()); cmd_count++;

				redisAppendCommand(thread_data->mp_redis_connection, "HSET user_%d nick %s", response.summary.id, request.summary.nick.c_str()); cmd_count++;

                redisAppendCommand(thread_data->mp_redis_connection, "SET usernick_%s %d", formatted_name.c_str(), response.summary.id); cmd_count++;
            } else {
                response.summary.nick = userDetails.nick;

                ApplyUserKeys(thread_data, "", request.summary, "", true);
                ApplyUserKeys(thread_data, "", request.summary, "custkey_");

                if(formatted_name.length() != 0) {
                    redisAppendCommand(thread_data->mp_redis_connection, "DEL usernick_%s", formatted_name.c_str()); cmd_count++;
                    redisAppendCommand(thread_data->mp_redis_connection, "SET usernick_%s %d", formatted_name.c_str(), response.summary.id); cmd_count++;
                }
            }

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE user_%d %d", response.summary.id, USER_EXPIRE_TIME); cmd_count++;

            if(formatted_name.length() != 0) {
                redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE usernick_%s %d", formatted_name.c_str(), USER_EXPIRE_TIME); cmd_count++;
            }

            for(int i=0;i<cmd_count;i++) {
                redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
                freeReplyObject(reply);
            }

            response.error_details.response_code = TaskShared::WebErrorCode_Success;
        } else {
            response.summary = GetUserSummaryByName(thread_data, formatted_name);
            response.error_details.response_code = TaskShared::WebErrorCode_UniqueNickInUse;
        }
        
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}
