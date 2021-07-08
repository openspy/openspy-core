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
	void Peer::OnWhois_FetchUser(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			std::ostringstream ss;
			
			ss << response_data.summary.username << " " << response_data.summary.GetIRCAddress(peer->IsUserAddressVisible(response_data.summary.id)) << " * :" << response_data.summary.realname;
			peer->send_numeric(311, ss.str(), true, response_data.summary.nick);

			ss.str("");

			std::vector<ChannelSummary>::iterator it = response_data.channel_summaries.begin();
			bool found_channels = false;
			while(it != response_data.channel_summaries.end()) {
				ChannelSummary summary = *(it++);			
				bool channel_visible = (summary.basic_mode_flags & (EChannelMode_Private|EChannelMode_Secret)) == 0;

				if(peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_IsInChannel || peer->GetOperFlags() & OPERPRIVS_GLOBALOWNER) {
					channel_visible = true;
				}
				if(peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_Invisible && (!peer->GetOperFlags() & OPERPRIVS_INVISIBLE)) { 
					channel_visible = false;
				}
				if(!channel_visible) continue;
				ss << summary.channel_name << " ";
				found_channels = true;
			}

			if(found_channels) {
				ss.str(ss.str().substr(0, ss.str().length() - 1));			
				peer->send_numeric(319, ss.str(), false, response_data.summary.nick);				
			}
			peer->send_numeric(318, "End of WHOIS list", false, response_data.summary.nick);
		}
		else {
			peer->send_no_such_target_error(response_data.profile.uniquenick);
			peer->send_numeric(318, "End of WHOIS list", false, response_data.profile.uniquenick);
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