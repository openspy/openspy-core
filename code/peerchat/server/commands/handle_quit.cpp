#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
    void Peer::send_quit(std::string reason) {

		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary;
		req.peer = this;
		req.summary = GetUserDetails();
	
		req.message_type = "QUIT";

		req.message = reason;

		std::map<int, int>::iterator it = m_channel_flags.begin();
		while (it != m_channel_flags.end()) {
			std::pair<int, int> p = *it;
			if(GetChannelFlags(p.first) & EUserChannelFlag_IsInChannel)
				req.channel_id_list.push_back(p.first);
			it++;
		}

		req.peer->IncRef();
		req.callback = NULL;
		
        scheduler->AddRequest(req.type, req);


		std::ostringstream s;
		s << "ERROR: Closing Link: " <<  ((Peerchat::Server *)GetDriver()->getServer())->getServerName() << " (" << reason << ")" << std::endl;
		SendPacket(s.str());
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