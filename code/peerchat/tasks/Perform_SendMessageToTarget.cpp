#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define USER_EXPIRE_TIME 300
namespace Peerchat {
    bool Handle_PrivMsg(TaskThreadData *thread_data, std::string message) {
        uint8_t *data_out;
		size_t data_len;

        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        OS::KVReader reader = OS::KVReader(message);

        
        OS::Base64StrToBin((const char *)reader.GetValue("message").c_str(), &data_out, data_len);

		bool includeSelf = (bool)reader.GetValueInt("includeSelf");
		int requiredChanFlags = reader.GetValueInt("requiredChanUserModes");
		int requiredOperFlags = reader.GetValueInt("requiredOperFlags");

		int onlyVisibleTo = reader.GetValueInt("onlyVisibleTo");
		std::string send_message = std::string((const char*)data_out, data_len);
		if (reader.HasKey("toChannelId")) {
			ChannelSummary summary = LookupChannelById(thread_data, reader.GetValueInt("toChannelId"));
			if (summary.channel_id) {
				UserSummary from_user_summary;
				// = LookupUserById(thread_data, reader.GetValueInt("fromUserId"));
				if(reader.HasKey("fromUserId")) {
					from_user_summary = LookupUserById(thread_data, reader.GetValueInt("fromUserId"));
				} else {
					from_user_summary = UserSummary::FromBase64String(reader.GetValue("fromUserSummary"));
				}
				ChannelUserSummary from;
				from.userSummary = from_user_summary;
				from.user_id = from_user_summary.id;
				from.channel_id = summary.channel_id;
				from.modeflags = LookupUserChannelModeFlags(thread_data, from.channel_id, from.user_id);

				ChannelUserSummary target;
				target.user_id = reader.GetValueInt("toUserId");
				if(reader.HasKey("toUserId")) {
					target.userSummary = LookupUserById(thread_data, reader.GetValueInt("toUserId"));
				} else if(reader.HasKey("toUserSummary")) {
					target.userSummary = UserSummary::FromBase64String(reader.GetValue("toUserSummary"));
				}
				
				target.channel_id = summary.channel_id;
				target.modeflags = LookupUserChannelModeFlags(thread_data, from.channel_id, target.user_id);
				server->OnChannelMessage(reader.GetValue("type"), from, summary, send_message, target, includeSelf, requiredChanFlags, requiredOperFlags, onlyVisibleTo);
			}
		}
		else {
			UserSummary from_user_summary, to_user_summary;

			if(reader.HasKey("fromUserId")) {
				from_user_summary = LookupUserById(thread_data, reader.GetValueInt("fromUserId"));
			} else {
				from_user_summary = UserSummary::FromBase64String(reader.GetValue("fromUserSummary"));
			}

			if(reader.HasKey("toUserId")) {
				to_user_summary = LookupUserById(thread_data, reader.GetValueInt("toUserId"));
			} else {
				to_user_summary = UserSummary::FromBase64String(reader.GetValue("toUserSummary"));
			}
			server->OnUserMessage(reader.GetValue("type"), from_user_summary, to_user_summary, send_message);
		}
        
        free(data_out);
		return true;
    }

	bool CheckUserCanSendMessage(ChannelSummary channel, Peer *peer) {
		int chan_user_flags = peer->GetChannelFlags(channel.channel_id);
		int has_vop = chan_user_flags & EUserChannelFlag_Voice || chan_user_flags & EUserChannelFlag_HalfOp || chan_user_flags & EUserChannelFlag_Op || chan_user_flags & EUserChannelFlag_Owner;
		if (channel.basic_mode_flags & EChannelMode_Moderated && !has_vop) {
			return false;
		}
		if (channel.basic_mode_flags & EChannelMode_NoOutsideMessages) {
			if (~chan_user_flags & EUserChannelFlag_IsInChannel) {
				return false;
			}
		}
		if (chan_user_flags & EUserChannelFlag_Gagged) {
			return false;
		}
		return true;
	}
    
    bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		std::string message = request.message;
		const char *base64 = OS::BinToBase64Str((uint8_t *)message.c_str(), message.length());
		std::string b64_string = base64;
		free((void *)base64);

		std::ostringstream mq_message;

		std::string target;
		if (request.message_target.length() > 0 && request.message_target[0] == '#') {
			target = request.message_target;
			//moderated check
			ChannelSummary summary = GetChannelSummaryByName(thread_data, target, false);
			if (summary.channel_id == 0 || !CheckUserCanSendMessage(summary, request.peer)) {
				if (summary.channel_id != 0) {
					request.peer->send_numeric(404, "Cannot send to channel", false, target);
				}
				else {
					request.peer->send_no_such_target_error(target);
				}
				request.peer->DecRef();
				return true;
			}
			mq_message << "\\type\\" << request.message_type.c_str() << "\\toChannelId\\" << summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id;
		}
		else {
			UserSummary to_summary = GetUserSummaryByName(thread_data, request.message_target);			
			mq_message << "\\type\\" << request.message_type.c_str() << "\\toUserId\\" << to_summary.id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id;
		}
	 	
        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());

		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}