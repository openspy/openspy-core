#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_UserJoinEvents(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        TaskResponse response;
        int initial_flags = request.channel_modify.set_mode_flags;
        if (initial_flags & EUserChannelFlag_Invisible) {
            std::ostringstream mq_message;
            std::string message = "INVISIBLE USER " + request.peer->GetUserDetails().nick + " JOINED CHANNEL";
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            mq_message << "\\type\\NOTICE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\requiredOperFlags\\" << OPERPRIVS_INVISIBLE << "\\includeSelf\\1";
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
        }
        if (initial_flags & EUserChannelFlag_Voice) {
            std::string message = "+v " + request.peer->GetUserDetails().nick;
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            std::ostringstream mode_message;
            mode_message << "\\type\\MODE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\includeSelf\\1";
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mode_message.str().c_str());
        }
        if (initial_flags & EUserChannelFlag_HalfOp || initial_flags & EUserChannelFlag_Op || initial_flags & EUserChannelFlag_Owner) {
            std::string message = "+o " + request.peer->GetUserDetails().nick;
            const char* base64 = OS::BinToBase64Str((uint8_t*)message.c_str(), message.length());
            std::string b64_string = base64;
            free((void*)base64);

            std::ostringstream mode_message;
            mode_message << "\\type\\MODE\\toChannelId\\" << request.channel_summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\-1\\includeSelf\\1";
            thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mode_message.str().c_str());
        }

		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}