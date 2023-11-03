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
    void Peer::OnTopic_FetchChannelInfo(TaskResponse response_data, Peer *peer) {
		std::ostringstream s;
		if(response_data.channel_summary.topic_time.tv_sec == 0) {
			peer->send_numeric(331, "No topic set", false, response_data.channel_summary.channel_name);
		} else {
			UserSummary topic_user_summary = response_data.channel_summary.topic_user_summary;
			peer->send_numeric(332, response_data.channel_summary.topic, false, response_data.channel_summary.channel_name);			
			s << topic_user_summary.nick << " " << response_data.channel_summary.topic_time.tv_sec;
			peer->send_numeric(333, s.str(), true, response_data.channel_summary.channel_name);			
		}
    }
	void Peer::send_topic(std::string channel) {
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_LookupChannelDetails;
		req.peer = this;
		req.channel_summary.channel_name = channel;
		req.peer->IncRef();
		req.callback = OnTopic_FetchChannelInfo;
		AddPeerchatTaskRequest(req);
	}
    void Peer::handle_topic(std::vector<std::string> data_parser) {
		if (data_parser.size() == 2) { //topic lookup
			send_topic(data_parser.at(1));
		}
		else { //topic set
			std::string message = data_parser.at(2);

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

			PeerchatBackendRequest req;
			req.type = EPeerchatRequestType_UpdateChannelModes;
			req.peer = this;
			req.summary = m_user_details;
			req.channel_summary.channel_name = data_parser.at(1);
			req.channel_modify.set_mode_flags = 0;
			req.channel_modify.unset_mode_flags = 0;

			req.channel_modify.update_topic = true;
			req.channel_modify.topic = message;
			req.peer->IncRef();
			req.callback = NULL;
			AddPeerchatTaskRequest(req);
		}
		
    }
}