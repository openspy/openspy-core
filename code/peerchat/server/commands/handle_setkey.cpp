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
	void Peer::OnSetKey(TaskResponse response_data, Peer* peer) {
		printf("setkey\n");
	}

    //SETKEY CHC 0 000 :\b_test\1
    void Peer::handle_setkey(std::vector<std::string> data_parser) {
        std::string user_target  = data_parser.at(1);

        std::string kv_string  = data_parser.at(4);
		bool do_combine = false;
		if (kv_string.length() > 1 && kv_string[0] == ':') {
			kv_string = kv_string.substr(1);
			do_combine = true;
		}
		if (do_combine) {
			for (int i = 5; i < data_parser.size(); i++) {
				kv_string = kv_string.append(" ").append(data_parser.at(i));
			}
		}

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetUserKeys;
        req.peer = this;
		req.summary.username = user_target;
		req.channel_modify.kv_data = kv_string;
        
        req.peer->IncRef();
        req.callback = OnSetKey;
        scheduler->AddRequest(req.type, req);
        
    }
}