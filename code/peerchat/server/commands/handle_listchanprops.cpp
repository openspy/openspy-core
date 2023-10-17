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
    void Peer::SerializeChanpropsRecord(ChanpropsRecord record, std::ostringstream& ss) {
        time_t time;
        char timestr[256];
        ss << "\\id\\" << record.id;
        ss << "\\chanmask\\" << record.channel_mask;
        ss << "\\password\\" << record.password;
        ss << "\\entrymsg\\" << record.entrymsg;
        ss << "\\comment\\" << record.comment;
        ss << "\\groupname\\" << record.groupname;
        ss << "\\limit\\" << record.limit;
        ss << "\\modeflags\\" << record.modeflags; //change to mode string

        ss << "\\onlyOwner\\" << record.onlyOwner;
        ss << "\\topic\\" << record.topic;
        ss << "\\setByNick\\" << record.setByNick;
        ss << "\\setByHost\\" << record.setByHost;
        ss << "\\setByPid\\" << record.setByPid;

        struct tm* timeinfo;
        time = record.set_at.tv_sec;
        timeinfo = localtime(&time);
        if (timeinfo != NULL) {
            strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
            ss << "\\setAt\\" << timestr;
        }

        time = record.expires_at.tv_sec;
        timeinfo = localtime(&time);
        if (time != 0 && timeinfo != NULL) {
            strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
            ss << "\\expiresAt\\" << timestr;
        }
    }
	void Peer::OnListChanProps(TaskResponse response_data, Peer* peer) {

        if (response_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
            ((Peer*)peer)->send_message("PRIVMSG", "Failed to list chanprops", *server_userSummary, ((Peer*)peer)->m_user_details.nick);
            return;
        }

        if(response_data.chanprops.id != 0) {
            std::ostringstream ss;
            ss << "LISTCHANPROPS ";
            SerializeChanpropsRecord(response_data.chanprops, ss);


            ((Peer*)peer)->send_message("PRIVMSG", ss.str(), *server_userSummary, ((Peer*)peer)->m_user_details.nick);
        }

        if(response_data.is_end) {
            ((Peer *)peer)->send_message("PRIVMSG", "LISTCHANPROPS \\final\\1", *server_userSummary, ((Peer *)peer)->m_user_details.nick);
        }
    }

    void Peer::handle_listchanprops(std::vector<std::string> data_parser) {
        std::string chanmask = data_parser.at(1);

		PeerchatBackendRequest req;

        req.type = EPeerchatRequestType_ListChanProps;
		
		req.peer = this;

		ChanpropsRecord chanpropsRecord;
		chanpropsRecord.channel_mask = chanmask;
		req.chanpropsRecord = chanpropsRecord;

		req.peer->IncRef();
		req.callback = OnListChanProps;
		AddPeerchatTaskRequest(req);

    }
}