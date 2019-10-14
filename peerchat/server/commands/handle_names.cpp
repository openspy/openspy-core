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