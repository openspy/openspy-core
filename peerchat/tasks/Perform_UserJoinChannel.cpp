#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
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
			}
		}
		return true;
	}

    bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, true);
		
        TaskResponse response;
		response.channel_summary = channel;

        if(!CheckUserCanJoinChannel(channel, request.peer, request.channel_summary.password)) {
            response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
        } else {
            response.error_details.response_code = TaskShared::WebErrorCode_Success;
			response.summary.id = EUserChannelFlag_IsInChannel;
        }
		
        if(request.callback)
            request.callback(response, request.peer);

		//needs to be after callback, due to IsInChannel flag needing to be set on user, for them to see their join message
		if (response.error_details.response_code == TaskShared::WebErrorCode_Success) {
			AddUserToChannel(thread_data, request.peer->GetUserDetails(), channel, response.summary.id);
		}

		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}