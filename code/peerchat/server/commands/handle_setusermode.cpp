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

		for (size_t i = 3; i < data_parser.size(); i++) {
			usermode_properties_string = usermode_properties_string.append(" ").append(data_parser.at(i));
		}

		OS::KVReader usermode_properties = usermode_properties_string;

		PeerchatBackendRequest req;

		UsermodeRecord usermodeRecord;
		usermodeRecord.isGlobal = 1; //default to global
		usermodeRecord.chanmask = channel_target;

		usermodeRecord.modeflags = 0;
		if (usermode_properties.HasKey("modeflags")) {
			usermodeRecord.modeflags = channelUserModesStringToFlags(usermode_properties.GetValue("modeflags"));
		}
		else if (usermode_properties.HasKey("mode")) {
			usermodeRecord.modeflags = channelUserModesStringToFlags(usermode_properties.GetValue("mode"));
		}
		if (usermodeRecord.modeflags == 0) {
			send_message("PRIVMSG", "No modeflags specified", *server_userSummary, m_user_details.nick);
		}

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

		usermodeRecord.expires_at.tv_nsec = 0;
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
		AddPeerchatTaskRequest(req);

    }
}