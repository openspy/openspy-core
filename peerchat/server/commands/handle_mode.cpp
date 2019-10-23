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
	void Peer::OnMode_FetchChannelInfo(TaskResponse response_data, Peer* peer) {
		std::ostringstream s;
		if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
			s << response_data.channel_summary.channel_name << " ";
			s << "+";
			for (int i = 0; i < num_mode_flags; i++) {
				if (response_data.channel_summary.basic_mode_flags & mode_flag_map[i].flag) {
					s << mode_flag_map[i].character;
				}
			}
			peer->send_numeric(324, s.str(), true);

			s.str("");
			s << response_data.channel_summary.channel_name << " " << response_data.channel_summary.created_at.tv_sec;
			peer->send_numeric(329, s.str(), true);
		}
		else if(response_data.error_details.response_code == TaskShared::WebErrorCode_NoSuchUser) {
			peer->send_no_such_target_error(response_data.channel_summary.channel_name);
		}
	}
	void Peer::handle_mode(std::vector<std::string> data_parser) {
		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		if (data_parser.size() == 2) {
			//lookup
			req.type = EPeerchatRequestType_LookupChannelDetails;
			req.peer = this;
			req.channel_summary.channel_name = data_parser.at(1);
			req.peer->IncRef();
			req.callback = OnMode_FetchChannelInfo;
			scheduler->AddRequest(req.type, req);
		}
		else if (data_parser.size() >= 3) { //set/unset
			// +m-n
			int last_offset = 2;
			
			bool set = true;
			int set_flags = 0, unset_flags = 0;
			std::string mode_string = data_parser.at(2);
			for (int i = 0; i < mode_string.length(); i++) {
				if (mode_string[i] == '+') {
					set = true;
				}
				else if (mode_string[i] == '-') {
					set = false;
				}
				else if(mode_string[i] == 'k') {
					req.channel_modify.update_password = true;
					if (set) {
						last_offset++;
						if (data_parser.size() <= last_offset) {
							send_numeric(666, "PARSE ERROR");
							return;
						}
						req.channel_modify.password = data_parser.at(last_offset);
					}
					else {
						req.channel_modify.password = "";
					}
				} else if(mode_string[i] == 'l') {
					req.channel_modify.update_limit = true;
					if (set) {
						last_offset++;
						if (data_parser.size() <= last_offset) {
							send_numeric(666, "PARSE ERROR");
							return;
						}
						req.channel_modify.limit = atoi(data_parser.at(last_offset).c_str());
					} else {
						req.channel_modify.limit = 0;
					}
				}
				else {
					ModeFlagMap flag;
					bool found = false;
					for (int x = 0; x < num_mode_flags; x++) {
						if (mode_flag_map[x].character == mode_string[i]) {
							flag = mode_flag_map[x];
							found = true;
							break;
						}
					}
					if (found) {
						if (set) {
							set_flags |= flag.flag;
							unset_flags &= ~flag.flag;
						}
						else {
							unset_flags |= flag.flag;
							set_flags &= ~flag.flag;
						}
					}
				}
			}
			req.type = EPeerchatRequestType_UpdateChannelModes;
			req.peer = this;
			req.summary = m_user_details;
			req.channel_summary.channel_name = data_parser.at(1);
			req.channel_modify.set_mode_flags = set_flags;
			req.channel_modify.unset_mode_flags = unset_flags;
			req.peer->IncRef();
			req.callback = OnMode_FetchChannelInfo;
			scheduler->AddRequest(req.type, req);
		}
	}
}