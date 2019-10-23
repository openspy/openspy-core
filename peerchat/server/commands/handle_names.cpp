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
    void Peer::OnNames_FetchChannelInfo(TaskResponse response_data, Peer *peer) {
		std::vector<ChannelUserSummary>::iterator it = response_data.channel_summary.users.begin();

		std::ostringstream s;
		std::string target = "= " + response_data.channel_summary.channel_name;
		while (it != response_data.channel_summary.users.end()) {
			ChannelUserSummary user = *it;
			s << user.userSummary.nick << " ";
			it++;
		}
		peer->send_numeric(353, s.str(), false, target);
		peer->send_numeric(366, "End of /NAMES list.");
    }
    void Peer::handle_names(std::vector<std::string> data_parser) {
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_LookupChannelDetails;
        req.peer = this;
        req.channel_summary.channel_name = data_parser.at(1);
        req.peer->IncRef();
        req.callback = OnNames_FetchChannelInfo;
        scheduler->AddRequest(req.type, req);
    }
}