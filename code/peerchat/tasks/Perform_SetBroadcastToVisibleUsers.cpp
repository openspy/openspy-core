#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_SetBroadcastToVisibleUsers(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        std::ostringstream channels;
        std::vector<int>::iterator it = request.channel_id_list.begin();
        while(it != request.channel_id_list.end()) {
            int id = *it;
            channels << id << ",";
            it++;
        }

        std::ostringstream channel_modes;
        it = request.channel_id_list.begin();
        while(it != request.channel_id_list.end()) {
            int modeflags = request.peer->GetChannelFlags(*it);
            channel_modes << modeflags << ",";
            it++;
        }

		std::string b64_string = "";

		if (request.message.length() != 0) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)request.message.c_str(), request.message.length());
			b64_string = base64;
			free((void*)base64);
		}


        std::string channel_str = channels.str();
        channel_str = channel_str.substr(0, channel_str.length() - 1);

        std::string channel_modes_str = channel_modes.str();
        channel_modes_str = channel_modes_str.substr(0, channel_modes_str.length() - 1);

		std::ostringstream mq_message;
		//mq_message << "\\type\\" << request.message_type.c_str()  << "\\message\\" << b64_string << "\\channels\\" << channel_str << "\\channel_modes\\" << channel_modes_str << "\\fromUserId\\" << request.summary.id << "\\includeSelf\\" << (bool)(request.type == EPeerchatRequestType_SetBroadcastToVisibleUsers);

        mq_message << "\\type\\" << request.message_type.c_str()  << "\\message\\" << b64_string << "\\channels\\" << channel_str << "\\channel_modes\\" << channel_modes_str;
        if(request.type == EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary) {
            mq_message << "\\fromSummary\\" << request.summary.ToBase64String(true);
        } else {
            mq_message << "\\fromUserId\\" << request.summary.id;
        }
        
        mq_message << "\\includeSelf\\" << (bool)(request.type == EPeerchatRequestType_SetBroadcastToVisibleUsers || request.type == EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary);
        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_broadcast_routingkey, mq_message.str().c_str());

        TaskResponse response;
        response.error_details.response_code = TaskShared::WebErrorCode_Success;
		if (request.callback) {
			request.callback(response, request.peer);
		}

		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}