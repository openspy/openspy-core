#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_SetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		UserSummary user_summary = GetUserSummaryByName(thread_data, request.summary.username);
		if (summary.channel_id != 0 && user_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			if (request.peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_IsInChannel) {
				std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
				std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
				while (it != iterators.second) {
					std::pair<std::string, std::string> p = *it;
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d_user_%d \"custkey_%s\" \"%s\"", summary.channel_id, user_summary.id, p.first.c_str(), p.second.c_str());
					it++;
				}
			}
			else { //not in channel
				response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
			}
		}
		else { //no such channel
			response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
		}

		


		response.channel_summary = summary;
		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}