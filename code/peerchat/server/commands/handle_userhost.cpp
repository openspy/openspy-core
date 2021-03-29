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
	void Peer::OnUsrip_FetchUser(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			std::ostringstream ss;
			ss << response_data.summary.nick << "=+" << response_data.summary.username << "@" << response_data.summary.hostname;
			peer->send_numeric(302, ss.str());
		}
		else if (response_data.error_details.response_code == TaskShared::WebErrorCode_NoSuchUser) {
			peer->send_no_such_target_error(response_data.profile.uniquenick);
		}
	}
	void Peer::handle_userhost(std::vector<std::string> data_parser) {
		if (data_parser.size() < 2) {
			std::ostringstream ss;
			ss << m_user_details.nick << "=+" << m_user_details.username << "@" << m_user_details.hostname;
			send_numeric(302, ss.str());
		}
		else {
            std::string target = data_parser.at(1);

            TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
            PeerchatBackendRequest req;
            req.type = EPeerchatRequestType_LookupUserDetailsByName;
            req.peer = this;
            req.summary.username = target;

            req.peer->IncRef();
            req.callback = OnUsrip_FetchUser;
            scheduler->AddRequest(req.type, req);
		}
	}
}