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
	void Peer::OnSetKey(TaskResponse response_data, Peer* peer) {
		
	}

    //SETKEY CHC 0 000 :\b_test\1
    void Peer::handle_setkey(std::vector<std::string> data_parser) {
        std::string kv_string  = data_parser.at(4);
		if (kv_string.length() > 1 && kv_string[0] == ':') {
			kv_string = kv_string.substr(1);
		}

        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetUserKeys;
        req.peer = this;
		req.summary.username = GetUserDetails().nick;
		req.channel_modify.kv_data = kv_string;
        
        req.peer->IncRef();
        req.callback = OnSetKey;
        AddPeerchatTaskRequest(req);
        
    }
}
