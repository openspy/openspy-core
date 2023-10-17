#include <sstream>

#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
    bool PerformDeleteServer(UTMasterRequest request, TaskThreadData *thread_data) {

        MMTaskResponse response;
        response.peer = request.peer;
        
        selectQRRedisDB(thread_data);
        
        OS::GameData game_info = request.peer->GetGameData();
        std::string server_key = GetServerKey_FromIPMap(request, thread_data, game_info);

        if(server_key.length() > 0) {
            //perform delete
            redisReply *reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "HSET %s deleted 1", server_key.c_str());
            if(reply) {
                freeReplyObject(reply);
            }
            
			std::ostringstream s;
			s << "\\del\\" << server_key.c_str();
            std::string str = s.str();
			sendAMQPMessage(mm_channel_exchange, mm_server_event_routingkey, str.c_str());
        }

        if(request.peer) {
            request.peer->DecRef();
        }
        if(request.callback) {
            request.callback(response);
        }

        return true;
    }
}
