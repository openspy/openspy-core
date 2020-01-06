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
	/*
<- :s 311 CHC \c2d\cdri\c8ver Xvaqlsf9fX|0 190.148.50.19 * :f338b38dac75de566f8272b2e674a6bf
<- :s 319 CHC \c2d\cdri\c8ver :#GPG!2386
<- :s 318 CHC \c2d\cdri\c8ver :End of WHOIS list
	*/
	void Peer::OnWhois_FetchUser(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			std::ostringstream ss;
			ss << response_data.summary.nick << " " << response_data.summary.username << " " << response_data.summary.hostname << " * :" << response_data.summary.realname;
			peer->send_numeric(311, ss.str(), true);

			ss.str("");
			std::vector<ChannelSummary>::iterator it = response_data.channel_summaries.begin();
			while(it != response_data.channel_summaries.end()) {
				ChannelSummary summary = *(it++);
				ss << summary.channel_name << " ";
			}

			ss.str(ss.str().substr(0, ss.str().length() - 1));			
			peer->send_numeric(319, ss.str());

			peer->send_numeric(318, "End of WHOIS list");
		}
		else {
			peer->send_no_such_target_error(response_data.profile.uniquenick);
		}
	}
    void Peer::handle_whois(std::vector<std::string> data_parser) {
        std::string target = data_parser.at(1);

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_LookupUserDetailsByName;
        req.peer = this;
        req.summary.username = target;
        
        req.peer->IncRef();
        req.callback = OnWhois_FetchUser;
        scheduler->AddRequest(req.type, req);
        
    }
}