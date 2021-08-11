#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
    bool Perform_UserPartChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		TaskResponse response;

		if(channel.channel_id != 0) {
			int requiredChanUserModes = 0;
			int from_mode_flags = LookupUserChannelModeFlags(thread_data, channel.channel_id, request.peer->GetBackendId());

			response.channel_summary = channel;


			if(from_mode_flags & EUserChannelFlag_IsInChannel) {
				if(from_mode_flags & EUserChannelFlag_Invisible) {
					requiredChanUserModes = EUserChannelFlag_Invisible;
					std::ostringstream mq_message;
					std::string message = "INVISIBLE USER " + request.summary.nick + " PARTED CHANNEL";
					const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
					std::string b64_string = base64;
					free((void*)base64);

					mq_message << "\\type\\NOTICE\\toChannelId\\" << channel.channel_id << "\\message\\" << b64_string << "\\fromUserSummary\\" << request.summary.ToBase64String(true) << "\\requiredChanUserModes\\" << EUserChannelFlag_Invisible << "\\includeSelf\\1";
					thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
				}
				RemoveUserFromChannel(thread_data, request.summary, channel, "PART", request.message, UserSummary(), false, requiredChanUserModes);
				response.error_details.response_code = TaskShared::WebErrorCode_Success;
			} else {
				response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
			}
		} else {
			response.channel_summary.channel_name = request.channel_summary.channel_name;
			response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
		}


		
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}