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
-> s PART #test :hello there
<- :CHC!CHC@99.243.125.215 PART #test :hello there
	*/
    void Peer::OnPartChannel(TaskResponse response_data, Peer* peer) {
        if(response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {            
            ((Peer *)peer)->SetChannelFlags(response_data.channel_summary.channel_id, 0);
        } else if(response_data.error_details.response_code == TaskShared::WebErrorCode_NoSuchUser) {
			((Peer *)peer)->send_no_such_target_error(response_data.channel_summary.channel_name);
		} else if (response_data.error_details.response_code == TaskShared::WebErrorCode_AuthInvalidCredentials) {
			((Peer *)peer)->send_numeric(403, "You're not on that channel", false, response_data.channel_summary.channel_name);
		}
    }
	void Peer::handle_part(std::vector<std::string> data_parser) {
		std::string target = data_parser.at(1);

		std::string message = "";
		if (data_parser.size() > 2) {
			message = data_parser.at(2);

			bool do_combine = false;
			if (message[0] == ':') {
				do_combine = true;
				message = message.substr(1);
			}

			if (do_combine) {
				for (size_t i = 3; i < data_parser.size(); i++) {
					message = message.append(" ").append(data_parser.at(i));
				}
			}
		}
		

		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_UserPartChannel;
		req.peer = this;
		req.channel_summary.channel_name = target;
		req.summary = GetUserDetails();
		req.message = message;
		req.peer->IncRef();
		req.callback = OnPartChannel;
		AddPeerchatTaskRequest(req);

	}
}