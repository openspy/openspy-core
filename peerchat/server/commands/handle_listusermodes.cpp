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
    void Peer::SerializeUsermodeRecord(UsermodeRecord record, std::ostringstream& ss) {
        time_t time;
        char timestr[256];
        ss << "\\usermodeid\\" << record.usermodeid;
        ss << "\\chanmask\\" << record.chanmask;
        ss << "\\modeflags\\" << modeFlagsToModeString(record.modeflags);

        if (record.hostmask.length() != 0)
            ss << "\\hostmask\\" << record.hostmask;
        if (record.machineid.length() != 0)
            ss << "\\machineid\\" << record.machineid;
        if (record.profileid != 0)
            ss << "\\profileid\\" << record.profileid;
        ss << "\\isGlobal\\" << record.isGlobal;

        if (record.expires_at.tv_sec != 0) {
            struct tm* timeinfo;
            time = record.expires_at.tv_sec;
            timeinfo = localtime(&time);
            if (timeinfo != NULL) {
                strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
                ss << "\\expires\\" << timestr;
            }
        }

        if (record.setByUserSummary.nick.length() != 0)
            ss << "\\setbynick\\" << record.setByUserSummary.nick;
        if (record.setByUserSummary.profileid != 0)
            ss << "\\setbypid\\" << record.setByUserSummary.profileid;
        if (record.setByUserSummary.hostname.length() != 0)
            ss << "\\setbyhost\\" << record.setByUserSummary.hostname;

        if (record.comment.length() != 0)
            ss << "\\comment\\" << record.comment;

        if (record.set_at.tv_sec != 0) {
            time = record.set_at.tv_sec;
            struct tm* timeinfo;
            timeinfo = localtime(&time);
            if (timeinfo != NULL) {
                strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
                ss << "\\setondate\\" << timestr;
            }

        }
    }
	void Peer::OnListUserModes(TaskResponse response_data, Peer* peer) {

        if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
            ((Peer*)peer)->send_message("PRIVMSG", "Failed to list usermodes", "SERVER!SERVER@*", ((Peer*)peer)->m_user_details.nick);
            return;
        }

        std::ostringstream ss;
        ss << "LISTUSERMODE ";
        SerializeUsermodeRecord(response_data.usermode, ss);

        ((Peer*)peer)->send_message("PRIVMSG", ss.str(), "SERVER!SERVER@*", ((Peer*)peer)->m_user_details.nick);

        if(response_data.is_end) {
            ((Peer *)peer)->send_message("PRIVMSG", "LISTUSERMODE \\final\\1", "SERVER!SERVER@*", ((Peer *)peer)->m_user_details.nick);
        }
    }

    void Peer::handle_listusermodes(std::vector<std::string> data_parser) {
        std::string chanmask = data_parser.at(1);

		TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;

		req.type = EPeerchatRequestType_ListUserModes;
		req.peer = this;

		UsermodeRecord usermodeRecord;
		usermodeRecord.chanmask = chanmask;
		req.usermodeRecord = usermodeRecord;

		req.peer->IncRef();
		req.callback = OnListUserModes;
		scheduler->AddRequest(req.type, req);

    }
}