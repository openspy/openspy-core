#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
	void Peer::OnSetChanKey(TaskResponse response_data, Peer* peer) {
		
	}

	//SETCHANKEY #GSP!godzilla2ps2!M411DDDD3M :\hostname\CDEMMNP|6\roomready\1\pwprivate\25

    void Peer::handle_setchankey(std::vector<std::string> data_parser) {
        std::string channel_target  = data_parser.at(1);

        std::string kv_string  = data_parser.at(2);
		bool do_combine = false;
		if (kv_string.length() > 1 && kv_string[0] == ':') {
			kv_string = kv_string.substr(1);
			do_combine = true;
		}
		if (do_combine) {
			for (size_t i = 3; i < data_parser.size(); i++) {
				kv_string = kv_string.append(" ").append(data_parser.at(i));
			}
		}

        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetChannelKeys;
        req.peer = this;
		req.channel_summary.channel_name = channel_target;
		req.channel_modify.kv_data = kv_string;
        
        req.peer->IncRef();
        req.callback = OnSetChanKey;
        AddPeerchatTaskRequest(req);
        
    }
}