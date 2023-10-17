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
	/*
        -> s GETKEY CHC 0 000 :\b_test
        <- :s 700 CHC CHC 0 :\1
	*/
	void Peer::OnGetKey(TaskResponse response_data, Peer* peer) {
		std::ostringstream ss;
        //uniquenick is "numeric", nick is target user

		if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			ss << response_data.summary.nick << " " << response_data.summary.nick << " " << response_data.profile.uniquenick << " :" << response_data.kv_data.ToString(true);
			peer->send_numeric(700, ss.str(), true, response_data.channel_summary.channel_name);
		} else {
			peer->send_no_such_target_error(response_data.profile.nick);
		}
	}

    void Peer::handle_getkey(std::vector<std::string> data_parser) {
        std::string user_target  = data_parser.at(1);
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


        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_GetUserKeys;
        req.peer = this;
		req.summary.username = user_target;
		req.channel_modify.kv_data = kv_data;

		req.profile.uniquenick = numeric;
        
        req.peer->IncRef();
        req.callback = OnGetKey;
        AddPeerchatTaskRequest(req);
        
    }
}