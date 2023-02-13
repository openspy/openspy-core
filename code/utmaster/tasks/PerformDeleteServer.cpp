#include <sstream>

#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
    bool PerformDeleteServer(UTMasterRequest request, TaskThreadData *thread_data) {

        MMTaskResponse response;
        response.peer = request.peer;
        
        OS::GameData game_info = request.peer->GetGameData();
        std::string server_key = GetServerKey_FromIPMap(request, thread_data, game_info);

        if(server_key.length() > 0) {
            //perform delete
            Redis::Command(thread_data->mp_redis_connection, 1, "HSET %s deleted 1", server_key.c_str());
			std::ostringstream s;
			s << "\\del\\" << server_key.c_str();
			thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
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