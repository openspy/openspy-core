#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {


    bool Handle_ChannelMessage(TaskThreadData *thread_data, std::string message) {
        OS::KVReader kvReader(message);
        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        int channel_id = 0, user_id = 0;
        ChannelSummary channel_summary;
        UserSummary summary;
		channel_id = kvReader.GetValueInt("channel_id");
		user_id = kvReader.GetValueInt("user_id");
		summary = LookupUserById(thread_data, user_id);
		channel_summary = LookupChannelById(thread_data, channel_id);

		server->OnChannelMessage(kvReader.GetValue("type"), summary.ToString(), channel_summary, "");
        return true;
    }

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

        if(!CheckUserCanJoinChannel(channel, request.peer, request.channel_summary.password)) {
            response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
        } else {
            AddUserToChannel(thread_data, request.peer->GetBackendId(), channel.channel_id);
            response.error_details.response_code = TaskShared::WebErrorCode_Success;
        }

        //
        

		
		response.channel_summary = channel;
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}