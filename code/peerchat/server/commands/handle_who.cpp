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
	void Peer::OnWho_FetchChannelInfo(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			peer->send_no_such_target_error(response_data.profile.uniquenick);
			return;
		}
		
		std::vector<ChannelUserSummary>::iterator it = response_data.channel_summary.users.begin();

		bool see_invisible = false; //XXX: get invisible operflag
		std::string target = response_data.channel_summary.channel_name;

		while (it != response_data.channel_summary.users.end()) {
			std::ostringstream s;

			ChannelUserSummary user = *(it++);
			if ((user.modeflags & EUserChannelFlag_Invisible) && !see_invisible) {
				continue;
			}

			if(response_data.channel_summary.basic_mode_flags & EChannelMode_Auditorium && user.userSummary.id != peer->GetBackendId()) {
				continue;
			} else if(response_data.channel_summary.basic_mode_flags & EChannelMode_Auditorium_ShowVOP && user.userSummary.id != peer->GetBackendId()) {
				if (!(user.modeflags & (EUserChannelFlag_Owner | EUserChannelFlag_Op | EUserChannelFlag_HalfOp | EUserChannelFlag_Voice))) {
					continue;
				}
			}

			s << user.userSummary.username << " ";
			s << user.userSummary.GetIRCAddress(peer->IsUserAddressVisible(user.userSummary.id)) << " ";
			s << ((Peerchat::Server*)peer->GetDriver()->getServer())->getServerName() << " ";
			s << user.userSummary.nick << " ";
			s << "H";
			if (user.modeflags & (EUserChannelFlag_Owner | EUserChannelFlag_Op | EUserChannelFlag_HalfOp)) {
				s << "@";
			}
			else if (user.modeflags & EUserChannelFlag_Voice) {
				s << "+";
			}

			s << " :0 " << user.userSummary.realname;
			peer->send_numeric(352, s.str(), true, target);
		}

		peer->send_numeric(315, "End of /WHO list.");
	}

	void Peer::OnWho_FetchUserInfo(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			peer->send_no_such_target_error(response_data.profile.uniquenick);
			peer->send_numeric(315, "End of /WHO list.", false, response_data.profile.uniquenick);	
		} else {
			UserSummary summary = response_data.summary;

			std::string target = summary.nick;

			std::ostringstream s;
			s << summary.username << " ";
			s << summary.hostname << " ";
			s << ((Peerchat::Server*)peer->GetDriver()->getServer())->getServerName() << " ";
			s << summary.nick << " ";
			s << "H";
			s << " :0 " << summary.realname;
			peer->send_numeric(352, s.str(), true, target);
			peer->send_numeric(315, "End of /WHO list.", false, target);	
		}


		

			
	}
    void Peer::handle_who(std::vector<std::string> data_parser) {
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
		std::string target = data_parser.at(1);

		if (target[0] == '#') {
			req.type = EPeerchatRequestType_LookupChannelDetails;
			req.peer = this;
			req.channel_summary.channel_name = target;
			req.peer->IncRef();
			req.callback = OnWho_FetchChannelInfo;
			scheduler->AddRequest(req.type, req);
		}
		else {
			req.type = EPeerchatRequestType_LookupUserDetailsByName;
			req.peer = this;
			req.summary.username = target;
			req.peer->IncRef();
			req.callback = OnWho_FetchUserInfo;
			scheduler->AddRequest(req.type, req);
		}
    }
}