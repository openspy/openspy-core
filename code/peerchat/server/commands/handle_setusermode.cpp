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
	void Peer::OnSetUserMode(TaskResponse response_data, Peer* peer) {
		if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			((Peer*)peer)->send_message("PRIVMSG", "Failed to set usermode", *server_userSummary, ((Peer*)peer)->m_user_details.nick);
			return;
		}

		std::ostringstream ss;
		ss << "SETUSERMODE ";
		SerializeUsermodeRecord(response_data.usermode, ss);

		((Peer*)peer)->send_message("PRIVMSG", ss.str(), *server_userSummary, ((Peer*)peer)->m_user_details.nick);

		if (response_data.is_end == 1) {
			((Peer*)peer)->send_message("PRIVMSG", "SETUSERMODE \\final\\1", *server_userSummary, ((Peer*)peer)->m_user_details.nick);
		}
	}

    void Peer::handle_setusermode(std::vector<std::string> data_parser) {
        std::string channel_target = data_parser.at(1);
		std::string usermode_properties_string = data_parser.at(2);
		if (usermode_properties_string[0] == ':') {
			usermode_properties_string = usermode_properties_string.substr(1);
		}

		for (int i = 3; i < data_parser.size(); i++) {
			usermode_properties_string = usermode_properties_string.append(" ").append(data_parser.at(i));
		}

		OS::KVReader usermode_properties = usermode_properties_string;


		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;

		UsermodeRecord usermodeRecord;
		usermodeRecord.chanmask = channel_target;
		usermodeRecord.modeflags = channelUserModesStringToFlags(usermode_properties.GetValue("modeflags"));
		if (usermode_properties.HasKey("hostmask")) {
			usermodeRecord.hostmask = usermode_properties.GetValue("hostmask");
		}

		if (usermode_properties.HasKey("comment")) {
			usermodeRecord.comment = usermode_properties.GetValue("comment");
		}

		if (usermode_properties.HasKey("machineid")) {
			usermodeRecord.machineid = usermode_properties.GetValue("machineid");
		}

		if (usermode_properties.HasKey("profileid")) {
			usermodeRecord.profileid = usermode_properties.GetValueInt("profileid");
		}

		if (usermode_properties.HasKey("isGlobal")) {
			usermodeRecord.isGlobal = usermode_properties.GetValueInt("isGlobal");
		}

		if (usermode_properties.HasKey("gameid")) {
			usermodeRecord.gameid = usermode_properties.GetValueInt("gameid");
			usermodeRecord.has_gameid = true;
		}

		usermodeRecord.expires_at.tv_usec = 0;
		usermodeRecord.expires_at.tv_sec = 0;
		if (usermode_properties.HasKey("expiressec")) {
			usermodeRecord.expires_at.tv_sec = usermode_properties.GetValueInt("expiressec");
		}

		usermodeRecord.setByUserSummary = GetUserDetails();


		req.usermodeRecord = usermodeRecord;

		req.type = EPeerchatRequestType_CreateUserMode;
		req.peer = this;
		req.peer->IncRef();
		req.callback = OnSetUserMode;
		scheduler->AddRequest(req.type, req);

    }
}