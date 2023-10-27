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

        TaskResponse response;
        response.summary = request.summary;
        response.profile.uniquenick = request.summary.nick;

        std::string formatted_name;
		std::transform(request.summary.nick.begin(),request.summary.nick.end(),std::back_inserter(formatted_name),tolower);

        std::string formatted_name_original;
        std::string original_nick = request.peer->GetUserDetails().nick;
		std::transform(original_nick.begin(),original_nick.end(),std::back_inserter(formatted_name_original),tolower);
        
        std::string updated_name_key = "usernick_" + formatted_name;
        std::string original_name_key = "usernick_" + formatted_name_original;

        UserSummary userDetails = request.peer->GetUserDetails();
        bool nick_update = false;
        if(userDetails.id != 0) {
            response.summary.id = userDetails.id;
        } else {
            response.summary.id = GetPeerchatUserID(thread_data);
            nick_update = true;
        }


        redisAppendCommand(thread_data->mp_redis_connection, "EXISTS %s", updated_name_key.c_str());
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

            std::ostringstream ss;
            ss << "user_" << response.summary.id;
            std::string user_key = ss.str();

            response.summary.nick = request.summary.nick;
            if(nick_update) {
                redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", original_name_key.c_str()); cmd_count++;

				redisAppendCommand(thread_data->mp_redis_connection, "HSET %s nick %s", user_key.c_str(), request.summary.nick.c_str()); cmd_count++;

                redisAppendCommand(thread_data->mp_redis_connection, "SET %s %d", updated_name_key.c_str(), response.summary.id); cmd_count++;
            } else {
                response.summary.nick = userDetails.nick;

                ApplyUserKeys(thread_data, "", request.summary, "", true);
                ApplyUserKeys(thread_data, "", request.summary, "_custkeys");

                if(formatted_name.length() != 0) {
                    redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", original_name_key.c_str()); cmd_count++;
                    redisAppendCommand(thread_data->mp_redis_connection, "SET %s %d", updated_name_key.c_str(), response.summary.id); cmd_count++;
                    redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", updated_name_key.c_str(), USER_EXPIRE_TIME); cmd_count++;
                }
            }

            redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", user_key.c_str(), USER_EXPIRE_TIME); cmd_count++;


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
