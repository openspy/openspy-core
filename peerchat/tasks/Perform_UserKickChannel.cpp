#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_UserKickChannel(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		UserSummary to_summary = GetUserSummaryByName(thread_data, request.message_target);

		TaskResponse response;
		response.summary.nick = request.message_target;

		if (to_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			RemoveUserFromChannel(thread_data, request.peer->GetUserDetails(), channel, "KICK", request.message, to_summary);
		}
		else {
			response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
		}


		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}