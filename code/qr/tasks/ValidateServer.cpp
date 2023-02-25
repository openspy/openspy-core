
#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"

#include <OS/gamespy/gsmsalg.h>

namespace MM {

    std::string GetV2CalculatedChallenge(MMPushRequest request, TaskThreadData *thread_data, std::string server_key) {
        
		Redis::Response reply;
		Redis::Value v;

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s challenge", server_key.c_str());
		if (Redis::CheckError(reply)) {
			return "";
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			std::string challenge = OS::strip_quotes(v.value._str).c_str();
            return challenge;
		}
        return "";
    }
    bool PerformValidate(MMPushRequest request, TaskThreadData *thread_data) {
        MMTaskResponse response;
        response.v2_instance_key = request.v2_instance_key;
        response.driver = request.driver;
        response.from_address = request.from_address;

        std::string server_key = GetServerKey_FromRequest(request, thread_data);

        if (server_key.empty()) {
            response.error_message = "Server not registered";
            request.callback(response);
            return true;
        }

        response.query_address = GetQueryAddressForServer(thread_data, server_key);

        std::string expected_challenge = GetV2CalculatedChallenge(request, thread_data, server_key);
        std::string chopped_challenge = request.gamename;
        if(chopped_challenge.length() > expected_challenge.length()) {
            chopped_challenge = chopped_challenge.substr(0, expected_challenge.length());
        }

        if(expected_challenge.compare(chopped_challenge) != 0) {
            response.error_message = "Invalid challenge response";
        } else if(isServerDeleted(thread_data, server_key, true)) {
            SetServerDeleted(thread_data, server_key, 0);
            Redis::Command(thread_data->mp_redis_connection, 0, "HDEL %s challenge", server_key.c_str());

            if(request.version != 1) { //version 1 new event is only after collection of first heartbeat
                std::ostringstream s;
                s << "\\new\\" << server_key.c_str();
                thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
            }
            
        }

        request.callback(response);

        return true;
    }

}