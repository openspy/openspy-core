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
	void Peer::OnSetChanProps(TaskResponse response_data, Peer* peer) {

        if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
            ((Peer*)peer)->send_message("PRIVMSG", "Failed to set chanprops", *server_userSummary, ((Peer*)peer)->m_user_details.nick);
            return;
        }

        std::ostringstream ss;
        ss << "SETCHANPROPS ";

        ss << "\\id\\" << response_data.chanprops.id;
        ss << "\\final\\1";

        ((Peer *)peer)->send_message("PRIVMSG",ss.str(), *server_userSummary, ((Peer *)peer)->m_user_details.nick);
    }

    ChanpropsRecord GetChanpropsFromKVReader(OS::KVReader reader) {
        ChanpropsRecord result;

        result.password = reader.GetValue("password");
        result.entrymsg = reader.GetValue("entrymsg");
        result.comment = reader.GetValue("comment");
        result.topic = reader.GetValue("topic");

        result.groupname = reader.GetValue("groupname");
        result.limit = reader.GetValueInt("limit");
        result.kickExisting = reader.GetValueInt("kickexisting");

        result.expires_at.tv_sec = reader.GetValueInt("expiresinsec");

        std::string modestring = reader.GetValue("mode");

        int modeflags = 0;
        for (int i = 0; i < num_channel_mode_flags; i++) {
            char m = channel_mode_flag_map[i].character;
            if(modestring.find(m) != std::string::npos) {
                modeflags |= channel_mode_flag_map[i].flag;
            }
        }

        result.modeflags = modeflags;
        return result;
    }

    void Peer::handle_setchanprops(std::vector<std::string> data_parser) {
        std::string chanmask = data_parser.at(1);

		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;

        req.type = EPeerchatRequestType_SetChanProps;
		
		req.peer = this;

		ChanpropsRecord record;
		
        std::string kvString = "";
        for (size_t i = 2; i < data_parser.size(); i++) {
			kvString = kvString.append(" ").append(data_parser.at(i));
		}
        kvString = kvString.substr(1);
        OS::KVReader kvReader = OS::KVReader(kvString);
        record = GetChanpropsFromKVReader(kvReader);

        UserSummary summary =  GetUserDetails();

        record.channel_mask = chanmask;        
        record.setByNick = summary.nick;
        record.setByPid = summary.profileid;
        record.setByHost = summary.hostname;

        req.chanpropsRecord = record;

		req.peer->IncRef();
		req.callback = OnSetChanProps;
		scheduler->AddRequest(req.type, req);
    }
}