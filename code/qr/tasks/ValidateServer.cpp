
#include <tasks/tasks.h>
#include <sstream>
#include "../server/v2.h"

#include <OS/gamespy/gsmsalg.h>

namespace MM {

	std::string GetCalculatedChallenge(MMPushRequest request, TaskThreadData *thread_data, std::string server_key) {
		
		redisReply *reply;
		std::string challenge = "";
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s challenge", server_key.c_str());
		if(reply) {
			if (reply->type == REDIS_REPLY_STRING) {
				challenge = OS::strip_quotes(reply->str);
			}
			freeReplyObject(reply);
			return challenge;
		}

		return "";
	}
	bool PerformValidate(MMPushRequest request, TaskThreadData *thread_data) {
		MMTaskResponse response;
		response.v2_instance_key = request.v2_instance_key;
		response.driver = request.driver;
		response.from_address = request.from_address;

		//selectQRRedisDB(thread_data);

		std::string server_key = GetServerKey_FromRequest(request, thread_data);

		if (server_key.empty()) {
			response.error_message = "Server not registered";
			request.callback(response);
			return true;
		}

		response.query_address = GetQueryAddressForServer(thread_data, server_key);
		response.challenge = server_key;

		std::string expected_challenge = GetCalculatedChallenge(request, thread_data, server_key);
		std::string chopped_challenge = request.gamename;
		if(chopped_challenge.length() > expected_challenge.length()) {
			chopped_challenge = chopped_challenge.substr(0, expected_challenge.length());
		}

		if(expected_challenge.compare(chopped_challenge) != 0) {
			response.error_message = "Invalid challenge response";
		} else if(isServerDeleted(thread_data, server_key, true)) {
			SetServerDeleted(thread_data, server_key, 0);

			if(request.version != 1) { //version 1 new event is only after collection of first heartbeat
				std::ostringstream s;
				s << "\\new\\" << server_key.c_str();

				IncrNumHeartbeats(thread_data, server_key);
				
				std::string msg = s.str();
				TaskShared::sendAMQPMessage(mm_channel_exchange, mm_server_event_routingkey, msg.c_str(), &request.from_address);
			}   
		} else if(request.version == 1) {
			// server already registered, no need to perform registration events again (such as key scan)
			// this scenario should only happen on v1, where both the game and query port get probed
			return true;
		}

		request.callback(response);

		return true;
	}

}
