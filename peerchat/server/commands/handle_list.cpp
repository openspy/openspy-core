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
	void Peer::getChannelSpecialInfo(std::ostringstream &ss, ChannelSummary summary) {
		if (summary.topic.length() > 0) {
			ss << "\\topic\\" << summary.topic;
		}
		if (summary.limit > 0) {
			ss << "\\limit\\" << summary.limit;
		}
		if (summary.password.length() > 0) {
			ss << "\\key\\1";
		}

		//XXX: dump channel keys
	}
	void Peer::OnListChannels(TaskResponse response_data, Peer* peer) {
		peer->send_numeric(321, "Channel :Users Name", true);
		std::vector<ChannelSummary>::iterator it = response_data.channel_summaries.begin();
		while (it != response_data.channel_summaries.end()) {
			std::ostringstream ss;
			ChannelSummary summary = *(it++);
			if (summary.basic_mode_flags & EChannelMode_Private || summary.basic_mode_flags & EChannelMode_Secret) {
				if (~peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_IsInChannel) {
					continue;
				}
				
			}
			ss << summary.channel_name << " " << summary.users.size() << " :";
			if (response_data.channel_summary.channel_id == -1) { //special info
				getChannelSpecialInfo(ss, summary);
			}
			else {
				ss << summary.topic;
			}
			
			peer->send_numeric(322, ss.str(), true);
		}
		peer->send_numeric(323, "End of /LIST");
		return;
	}
	void Peer::handle_list(std::vector<std::string> data_parser) {
		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.channel_modify.update_topic = false;
		req.channel_summary.channel_id = 0;
		req.channel_summary.limit = 0;

		std::string target = "*";
		if (data_parser.size() > 1) {
			target = data_parser.at(1);
			req.channel_summary.channel_name = target;

			if (target.size() > 0 && target[0] == 'k') {
				req.channel_summary.channel_id = -1;
			}
			else if (data_parser.size() > 2) {
				req.channel_summary.channel_id = -1;
			}
		}

		req.type = EPeerchatRequestType_ListChannels;
		req.peer = this;


		req.peer->IncRef();
		req.callback = OnListChannels;
		scheduler->AddRequest(req.type, req);

	}
	void Peer::handle_listlimit(std::vector<std::string> data_parser) {
		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.channel_modify.update_topic = false;
		req.channel_summary.channel_id = 0;

		std::string target = "*";
		if (data_parser.size() > 3) {
			target = data_parser.at(2);
		}
		req.channel_summary.channel_name = target;

		int limit = 0;
		if (data_parser.size() > 2) {
			limit = atoi(data_parser.at(1).c_str());
			req.channel_summary.limit = limit;
		}

		req.type = EPeerchatRequestType_ListChannels;
		req.peer = this;


		req.peer->IncRef();
		req.callback = OnListChannels;
		scheduler->AddRequest(req.type, req);

	}
}