#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

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
	int Peer::GetListUserCount(ChannelSummary summary) {
		if(GetOperFlags() & OPERPRIVS_INVISIBLE) {
			return summary.users.size();
		}
		std::vector<ChannelUserSummary>::iterator it = summary.users.begin();
		int c = 0;
		while (it != summary.users.end()) {
			ChannelUserSummary user = *(it++);
			if (user.modeflags & EUserChannelFlag_Invisible) {
				continue;
			}
			c++;
		}
		return c;
		
	}
	void Peer::OnListChannels(TaskResponse response_data, Peer* peer) {
		peer->send_numeric(321, "Channel :Users Name", true);
		std::vector<ChannelSummary>::iterator it = response_data.channel_summaries.begin();
		while (it != response_data.channel_summaries.end()) {
			std::ostringstream ss;
			ChannelSummary summary = *(it++);
			int user_count = peer->GetListUserCount(summary);
			if (!(peer->GetOperFlags() & OPERPRIVS_OPEROVERRIDE)) {
				if (summary.basic_mode_flags & EChannelMode_Private || summary.basic_mode_flags & EChannelMode_Secret) {
					if (~peer->GetChannelFlags(summary.channel_id) & EUserChannelFlag_IsInChannel) {
						continue;
					}
				}
			}
			if(user_count == 0 && !(summary.basic_mode_flags & EChannelMode_StayOpen)) {
				continue;
			}
			ss << summary.channel_name << " " << user_count << " :";
			if (response_data.summary.id == 1) { //special info
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
		PeerchatBackendRequest req;

		std::string target = "*";
		size_t command_index = 1;

		bool got_filtermask = false, got_keydump = false;
		if(data_parser.size() > 1) {
			do {
				std::string s = data_parser.at(command_index);
				if(s.compare("k") == 0 && command_index+1 == data_parser.size()) {
					got_keydump = true;
				} else if(!got_filtermask) {
					target = s;
					got_filtermask = true;
				}
			} while(++command_index < data_parser.size());
		}
		

		req.channel_summary.channel_id = got_keydump;
		req.channel_summary.channel_name = target;

		req.type = EPeerchatRequestType_ListChannels;
		req.peer = this;


		req.peer->IncRef();
		req.callback = OnListChannels;
		AddPeerchatTaskRequest(req);
	}
	void Peer::handle_listlimit(std::vector<std::string> data_parser) {
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
		AddPeerchatTaskRequest(req);

	}
}