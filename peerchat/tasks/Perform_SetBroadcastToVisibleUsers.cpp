#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
    /*
		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetBroadcastToVisibleUsers;
		req.peer = this;
		req.summary = GetUserDetails();
		req.message_target = newNick;
		req.message_type = "NICK";

		std::map<int, int>::iterator it = m_channel_flags.begin();
		while (it != m_channel_flags.end()) {
			std::pair<int, int> p = *it;
			req.channel_id_list.push_back(p.first);
			it++;
		}

		req.peer->IncRef();
		req.callback = NULL;
		scheduler->AddRequest(req.type, req);
    */
	bool Perform_SetBroadcastToVisibleUsers(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        std::ostringstream channels;
        std::vector<int>::iterator it = request.channel_id_list.begin();
        while(it != request.channel_id_list.end()) {
            int id = *it;
            channels << id << ",";
            it++;
        }
		std::string b64_string = "";

		if (request.message.length() != 0) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)request.message.c_str(), request.message.length());
			b64_string = base64;
			free((void*)base64);
		}


        std::string channel_str = channels.str();
        channel_str = channel_str.substr(0, channel_str.length()-1);

		std::ostringstream mq_message;
		mq_message << "\\type\\" << request.message_type.c_str()  << "\\message\\" << b64_string << "\\channels\\" << channel_str << "\\source\\" << request.summary.ToString() << "\\includeSelf\\" << (bool)(request.type == EPeerchatRequestType_SetBroadcastToVisibleUsers);
        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_broadcast_routingkey, mq_message.str().c_str());

		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}