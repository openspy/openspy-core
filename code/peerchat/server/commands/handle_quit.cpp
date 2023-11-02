#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
	void Peer::OnQuit_TaskComplete(TaskResponse response_data, Peer *peer) {
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_DeleteUser;
		req.peer = NULL;
		req.summary = response_data.summary;
		req.callback = NULL;
		AddPeerchatTaskRequest(req);
	}
    void Peer::send_quit(std::string reason) {

		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary;
		req.summary = GetUserDetails();
		req.message_type = "QUIT";
		req.message = reason;
		req.channel_flags = GetChannelFlagsMap();

		std::map<int, int>::iterator it = m_channel_flags.begin();
		while (it != m_channel_flags.end()) {
			std::pair<int, int> p = *it;
			if(GetChannelFlags(p.first) & EUserChannelFlag_IsInChannel)
				req.channel_id_list.push_back(p.first);
			it++;
		}

		req.callback = OnQuit_TaskComplete;
		
        AddPeerchatTaskRequest(req);
    }
    void Peer::handle_quit(std::vector<std::string> data_parser) {
        std::string reason = "";
        if(data_parser.size() > 1) {
            reason = data_parser.at(1);
            
			bool do_combine = false;
			if (reason[0] == ':') {
				do_combine = true;
				reason = reason.substr(1);
			}

			if (do_combine && data_parser.size() > 2) {
				for (size_t i = 2; i < data_parser.size(); i++) {
					reason = reason.append(" ").append(data_parser.at(i));
				}
			}
        }
        Delete(false, reason);
    }
}