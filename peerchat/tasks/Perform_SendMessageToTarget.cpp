#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define USER_EXPIRE_TIME 300
namespace Peerchat {
    bool Handle_Message(TaskThreadData *thread_data, std::string message) {
        uint8_t *data_out;
		size_t data_len;

        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        OS::KVReader reader = OS::KVReader(message);

        
        OS::Base64StrToBin((const char *)reader.GetValue("message").c_str(), &data_out, data_len);
		std::string to = reader.GetValue("to");
		
		std::string send_message = std::string((const char*)data_out, data_len);
		if (to.length() > 0 && to[0] == '#') {
			ChannelSummary summary = GetChannelSummaryByName(thread_data, to, false);
			if (summary.channel_id) {
				server->OnChannelMessage(reader.GetValue("type"), reader.GetValue("from"), summary, send_message);
			}
			
		}
		else {
			server->OnUserMessage(reader.GetValue("type"), reader.GetValue("from"), to, send_message);
		}
        
        free(data_out);
		return true;
    }

	bool CheckUserCanSendMessage(ChannelSummary channel, Peer *peer) {
		if (channel.basic_mode_flags & EChannelMode_Moderated) {
			return false;
		}
		if (channel.basic_mode_flags & EChannelMode_NoOutsideMessages) {
			if (~peer->GetChannelFlags(channel.channel_id) & EUserChannelFlag_IsInChannel) {
				return false;
			}
		}
		return true;
	}
    
    bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		std::string message = ":" + request.message;
		const char *base64 = OS::BinToBase64Str((uint8_t *)message.c_str(), message.length());
		std::string b64_string = base64;
		free((void *)base64);

		std::string target;
		if (request.message_target.length() > 0 && request.message_target[0] == '#') {
			target = request.message_target;
			//moderated check
			ChannelSummary summary = GetChannelSummaryByName(thread_data, target, false);
			if (!CheckUserCanSendMessage(summary, request.peer)) {
				request.peer->send_numeric(404, "Cannot send to channel", false, target);
				request.peer->DecRef();
				return true;

			}
		}
		else {
			UserSummary to_summary = GetUserSummaryByName(thread_data, request.message_target);
			target = to_summary.ToString();
		}
		

		std::ostringstream mq_message;
		mq_message << "\\type\\" << request.message_type.c_str() << "\\to\\" << target << "\\message\\" << b64_string << "\\from\\" << request.summary.ToString();


        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());

		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}