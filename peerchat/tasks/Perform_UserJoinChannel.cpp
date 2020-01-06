#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool CheckUserCanJoinChannel(ChannelSummary channel, Peer *peer, std::string password) {
		if (channel.basic_mode_flags & EChannelMode_InviteOnly) {
			if (~peer->GetChannelFlags(channel.channel_id) & EUserChannelFlag_Invited) {
				peer->send_numeric(473, "Cannot join channel (+i)", false, channel.channel_name);
				return false;
			}
		}
		if (channel.limit != 0 && channel.users.size() >= channel.limit) {
			peer->send_numeric(471, "Cannot join channel (+l)", false, channel.channel_name);
			return false;
		}
		if (channel.password.length() != 0) {
			if (channel.password.compare(password) != 0) {
				peer->send_numeric(475, "Cannot join channel (+k)", false, channel.channel_name);
				return false;
			}
		}
		return true;
	}

    bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, true);
		std::string original_password = request.channel_summary.password;
        TaskResponse response;
		response.channel_summary = channel;

		int initial_flags = EUserChannelFlag_IsInChannel;

		if (request.channel_modify.set_mode_flags & EUserMode_Quiet) {
			initial_flags |= EUserChannelFlag_Quiet;
		}
		if (request.channel_modify.set_mode_flags & EUserMode_Invisible) {
			initial_flags |= EUserChannelFlag_Invisible;
		}

        if(!CheckUserCanJoinChannel(channel, request.peer, original_password)) {
            response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
        } else {
            response.error_details.response_code = TaskShared::WebErrorCode_Success;
			response.summary.id = initial_flags;
        }
		
		if (response.error_details.response_code == TaskShared::WebErrorCode_Success) {
			AddUserToChannel(thread_data, request.peer->GetUserDetails(), channel, initial_flags);

			if (initial_flags & EUserChannelFlag_Invisible) {
				std::ostringstream mq_message;
				std::string message = "INVISIBLE USER " + request.peer->GetUserDetails().nick + " JOINED CHANNEL";
				const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
				std::string b64_string = base64;
				free((void*)base64);

				mq_message << "\\type\\NOTICE\\toChannelId\\" << channel.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.peer->GetBackendId() << "\\requiredChanUserModes\\" << EUserChannelFlag_Invisible << "\\includeSelf\\1";
				thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
			}
		}

		if (request.callback)
			request.callback(response, request.peer);

		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}