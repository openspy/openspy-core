#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_SetUserKeys(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		std::map<std::string, std::string> broadcast_keys;
		UserSummary user_summary = GetUserSummaryByName(thread_data, request.summary.username);

		if (user_summary.id != request.peer->GetBackendId()) {
			if(!(request.peer->GetOperFlags() & OPERPRIVS_GLOBALOWNER)) {
				return true;
			}
		}

        std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
        std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
		int cmd_count = 0;
		std::ostringstream ss;
		ss << "user_" << user_summary.id << "_custkeys";
		std::string user_key = ss.str();

        while (it != iterators.second) {
            std::pair<std::string, std::string> p = *it;
            redisAppendCommand(thread_data->mp_redis_connection, "HSET %s %s %s", user_key.c_str(), p.first.c_str(), p.second.c_str());
			cmd_count++;
            it++;
        }

		for(int i = 0;i < cmd_count;i++) {
			void *reply;
			redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			freeReplyObject(reply);
		}

		ApplyUserKeys(thread_data, "", user_summary, "", true);
		ApplyUserKeys(thread_data, "", user_summary, "_custkeys");

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}
