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

	void Peer::OnListChannels(TaskResponse response_data, Peer* peer) {

		peer->send_numeric(321, "Channel :Users Name", true);
		std::vector<ChannelSummary>::iterator it = response_data.channel_summaries.begin();
		std::ostringstream ss;
		while (it != response_data.channel_summaries.end()) {
			ChannelSummary summary = *(it++);
			if (summary.basic_mode_flags & EChannelMode_Private || summary.basic_mode_flags & EChannelMode_Secret) {
				if (~peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_IsInChannel) {
					continue;
				}
				
			}
			ss << summary.channel_name << " " << summary.users.size() << " :" << summary.topic;
			peer->send_numeric(322, ss.str(), true);
		}
		peer->send_numeric(323, "End of /LIST");
		return;
	}
    void Peer::handle_list(std::vector<std::string> data_parser) {
        std::string target = "*";
        if(data_parser.size() > 1) {
            target = data_parser.at(1);
        }

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_ListChannels;
        req.peer = this;
        req.channel_summary.channel_name = target;
        
        req.peer->IncRef();
        req.callback = OnListChannels;
        scheduler->AddRequest(req.type, req);
        
    }
}