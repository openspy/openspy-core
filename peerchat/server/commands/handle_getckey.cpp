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
<- :s 702 CHC #STAFF CHC 3 :\1
<- :s 703 CHC #STAFF 3 :End of GETCKEY
	*/
	void Peer::OnGetCKey(TaskResponse response_data, Peer* peer) {
		std::ostringstream ss;
		ss << response_data.summary.nick << " " << response_data.profile.uniquenick << " :" << OS::MapToKVString(response_data.kv_data);
		peer->send_numeric(702, ss.str(), true, response_data.channel_summary.channel_name);


		if (response_data.profile.id == 1) {
			ss.str("");
			ss << response_data.profile.uniquenick << " :End of GETCKEY";
			peer->send_numeric(703, ss.str(), true, response_data.channel_summary.channel_name);
		}
	}
	// getckey $chan $me 3 002 :\b_test
    void Peer::handle_getckey(std::vector<std::string> data_parser) {
        std::string channel_target = data_parser.at(1);
        std::string user_target  = data_parser.at(2);
		std::string numeric = data_parser.at(3);

        std::string kv_string  = data_parser.at(5);
		if (kv_string.length() > 1 && kv_string[0] == ':') {
			kv_string = kv_string.substr(1);
		}
		std::vector<std::string> kv_data = OS::KeyStringToVector(kv_string);
		std::map<std::string, std::string> kv_data_map;

		std::vector<std::string>::iterator it = kv_data.begin();
		while (it != kv_data.end()) {
			std::string s = *it;
			kv_data_map[s] = "";
			it++;
		}


        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_GetChannelKeys;
        req.peer = this;
		req.channel_summary.channel_name = channel_target;
		req.summary.username = user_target;
		req.channel_modify.kv_data = kv_data_map;

		req.profile.uniquenick = numeric;
        
        req.peer->IncRef();
        req.callback = OnGetCKey;
        scheduler->AddRequest(req.type, req);
        
    }
}