#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool CheckUserCanJoinChannel(TaskThreadData *thread_data, ChannelSummary channel, Peer *peer, std::string password, int initial_flags) {
		if(peer->GetOperFlags() & OPERPRIVS_OPEROVERRIDE) {
			return true;
		}
		if(CountUserChannels(thread_data, peer->GetBackendId()) > MAX_USER_CHANNELS) {
			peer->send_numeric(405, "You have joined too many channels", false, channel.channel_name);
			return false;
		}
		if (channel.basic_mode_flags & EChannelMode_InviteOnly) {
			if (~peer->GetChannelFlags(channel.channel_id) & EUserChannelFlag_Invited || initial_flags & EUserChannelFlag_Invited) {
				peer->send_numeric(473, "Cannot join channel (+i)", false, channel.channel_name);
				return false;
			}
		}
		if (channel.limit != 0 && channel.users.size() >= (size_t)channel.limit) {
			if((channel.basic_mode_flags & EChannelMode_OpsObeyChannelLimit) || GetUserChannelModeLevel(initial_flags) < 3) {
				peer->send_numeric(471, "Cannot join channel (+l)", false, channel.channel_name);
				return false;
			}
			
		}
		if (channel.password.length() != 0) {
			if (channel.password.compare(password) != 0) {
				peer->send_numeric(475, "Cannot join channel (+k)", false, channel.channel_name);
				return false;
			}
		}
		if(initial_flags & EUserChannelFlag_Banned) {
			peer->send_numeric(474, "Cannot join channel (+b)", false, channel.channel_name);
			return false;
		}
		return true;
	}

    bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		bool created = false;
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, true, request.peer->GetUserDetails(), &created);
		std::string original_password = request.channel_summary.password;
        TaskResponse response;
		response.channel_summary = channel;

		int initial_flags = LookupUserChannelModeFlags(thread_data, channel.channel_id, request.peer->GetBackendId());

		bool is_in_channel = false;

		if(initial_flags & EUserChannelFlag_IsInChannel) {
			is_in_channel = true;
		} else {
			initial_flags |= EUserChannelFlag_IsInChannel;
		}
		initial_flags |= request.channel_modify.set_mode_flags;

		UserSummary userSummary = request.summary;
		
		if(!is_in_channel) {
			if(channel.basic_mode_flags & EChannelMode_UserCreated && created) {
				Create_StagingRoom_UsermodeRecords(channel, request, thread_data);
			}

			initial_flags |= getEffectiveUsermode(thread_data, channel.channel_id, userSummary, request.peer);

			if(!is_in_channel && !CheckUserCanJoinChannel(thread_data, channel, request.peer, original_password, initial_flags)) {
				response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
			} else {
				response.error_details.response_code = TaskShared::WebErrorCode_Success;
				response.summary.id = initial_flags;
			}
			
			if (response.error_details.response_code == TaskShared::WebErrorCode_Success) {
				AddUserToChannel(thread_data, userSummary, channel, initial_flags);
			}
		} else {
			response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
		}

		if (request.callback)
			request.callback(response, request.peer);

		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}
