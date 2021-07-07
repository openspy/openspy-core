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
	void Peer::OnGetChanKey(TaskResponse response_data, Peer* peer) {
		std::ostringstream ss;
		ss << response_data.channel_summary.channel_name << " " << response_data.profile.uniquenick << " :" << response_data.kv_data.ToString(true);
		peer->send_numeric(704, ss.str(), true);
	}

	// getckey $chan $me 3 002 :\b_test
    void Peer::handle_getchankey(std::vector<std::string> data_parser) {
        std::string channel_target = data_parser.at(1);
		std::string numeric = data_parser.at(2);

        std::string kv_string  = data_parser.at(4);
		if (kv_string.length() > 1 && kv_string[0] == ':') {
			kv_string = kv_string.substr(1);
		}
		OS::KVReader kv_data;
		std::ostringstream ss;
		std::vector<std::string> key_list = OS::KeyStringToVector(kv_string);
		std::vector<std::string>::iterator it = key_list.begin();
		while (it != key_list.end()) {
			ss << "\\" << *(it++) << "\\stub";
		}

		kv_data = ss.str();


        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_GetChannelKeys;
        req.peer = this;
		req.channel_summary.channel_name = channel_target;
		req.channel_modify.kv_data = kv_data;

		req.profile.uniquenick = numeric;
        
        req.peer->IncRef();
        req.callback = OnGetChanKey;
        scheduler->AddRequest(req.type, req);
        
    }
}