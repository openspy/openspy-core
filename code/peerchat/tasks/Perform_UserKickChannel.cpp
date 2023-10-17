#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_UserKickChannel(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		UserSummary to_summary = GetUserSummaryByName(thread_data, request.message_target);

		int from_mode_flags = LookupUserChannelModeFlags(thread_data, channel.channel_id, request.peer->GetBackendId());
		int to_mode_flags = LookupUserChannelModeFlags(thread_data, channel.channel_id, to_summary.id);

		

		TaskResponse response;
		response.summary.nick = request.message_target;

		if (!CheckActionPermissions(request.peer, request.channel_summary.channel_name, from_mode_flags, to_mode_flags, (int)EUserChannelFlag_HalfOp)) {
			response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
		}
		else {
			if (to_summary.id != 0) {
				response.error_details.response_code = TaskShared::WebErrorCode_Success;
				int requiredChanUserModes = 0;
				if(to_mode_flags & EUserChannelFlag_Invisible) {
					requiredChanUserModes = EUserChannelFlag_Invisible;
					std::ostringstream mq_message;
					std::string message = "INVISIBLE USER " + to_summary.nick + " KICKED FROM CHANNEL";
					const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
					std::string b64_string = base64;
					free((void*)base64);

					mq_message << "\\type\\NOTICE\\toChannelId\\" << channel.channel_id << "\\message\\" << b64_string << "\\fromUserSummary\\" << to_summary.ToBase64String(true) << "\\requiredChanUserModes\\" << EUserChannelFlag_Invisible << "\\includeSelf\\1";
					sendAMQPMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
				}
				
				RemoveUserFromChannel(thread_data, request.summary, channel, "KICK", request.message, to_summary, false, requiredChanUserModes);
			}
			else {
				response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
			}
		}


		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}