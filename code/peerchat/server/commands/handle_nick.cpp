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
	void Peer::SendNickUpdate(std::string newNick) {
		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary;
		req.peer = this;
		req.summary = GetUserDetails();
		req.message = newNick;
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
	}
    void Peer::OnNickReserve(TaskResponse response_data, Peer *peer) {
        if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            peer->m_user_details.id = response_data.summary.id;
            if (peer->m_sent_client_init) {
                peer->SendNickUpdate(response_data.summary.nick);
                peer->m_user_details.nick = response_data.summary.nick;
            }
            else {
                peer->m_user_details.nick = response_data.summary.nick;
                peer->OnUserMaybeRegistered();
            }
        } 
        else if (response_data.error_details.response_code == TaskShared::WebErrorCode_UniqueNickInUse) {
            peer->send_numeric(433,"Nickname is already in use", false, response_data.profile.uniquenick);
        }
    }
    void Peer::handle_nick(std::vector<std::string> data_parser) {
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetUserDetails;
        req.peer = this;
        req.summary = GetUserDetails();

		std::string nick = data_parser.at(1);
		if (nick[0] == ':') {
			nick = nick.substr(1);
		}

        if(nick.compare("*") == 0) {
            nick = ((Peer *)peer)->m_profile.uniquenick;
        }
        req.summary.nick = nick;
        req.peer->IncRef();
        req.callback = OnNickReserve;
        scheduler->AddRequest(req.type, req);
    }
}