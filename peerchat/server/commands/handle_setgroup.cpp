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
	void Peer::OnSetGroup(TaskResponse response_data, Peer* peer) {
		printf("setgroup\n");
	}

    void Peer::handle_setgroup(std::vector<std::string> data_parser) {
        std::string channel_target = data_parser.at(1);
        std::string group  = data_parser.at(2);

        std::ostringstream s;
        s << "\\GROUP\\" << group;
        std::string kv_string = s.str();

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetChannelKeys;
        req.peer = this;
		req.channel_summary.channel_name = channel_target;
		req.channel_modify.kv_data = kv_string;
        
        req.peer->IncRef();
        req.callback = OnSetGroup;
        scheduler->AddRequest(req.type, req);
    }
}