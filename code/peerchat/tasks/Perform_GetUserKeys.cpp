#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_GetUserKeys(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		redisReply *reply;
		TaskResponse response;

		UserSummary user_summary = GetUserSummaryByName(thread_data, request.summary.username);
		response.summary = user_summary;
		response.profile.uniquenick = request.profile.uniquenick; //getkey numeric value
		response.profile.nick = request.summary.username;

		std::ostringstream ss;

		if (user_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET user_%d custkey_%s", user_summary.id, p.first.c_str());
				if (reply == NULL) {
					continue;
				}
					ss << "\\" << p.first << "\\";
				if (reply->type == REDIS_REPLY_STRING) {
					ss << reply->str;
				}
			}
		}

		response.kv_data = ss.str();

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}