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
	void Peer::OnKickCallback(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code == TaskShared::WebErrorCode_NoSuchUser) {
			peer->send_no_such_target_error(response_data.summary.nick);
		}
	}
	/*
-> s KICK #test CHC :hello there
<- :CHC!CHC@* KICK #test CHC :hello there
	*/
	void Peer::handle_kick(std::vector<std::string> data_parser) {
		
		std::string channel_target = data_parser.at(1);
        std::string user_target = data_parser.at(2);

		std::string message = "";
		if (data_parser.size() > 4) {
			message = data_parser.at(3);
			bool do_combine = false;
			if (message[0] == ':') {
				do_combine = true;
				message = message.substr(1);
			}

			if (do_combine) {
				for (size_t i = 4; i < data_parser.size(); i++) {
					message = message.append(" ").append(data_parser.at(i));
				}
			}
		}

		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_UserKickChannel;
		req.peer = this;
		req.channel_summary.channel_name = channel_target;
		req.summary = GetUserDetails();
        req.message_target = user_target;
		req.message = message;
		req.peer->IncRef();
		req.callback = OnKickCallback;
		AddPeerchatTaskRequest(req);

	}
}
