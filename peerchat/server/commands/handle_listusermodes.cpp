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
	void Peer::OnListUserModes(TaskResponse response_data, Peer* peer) {
        char timestr[256];

        std::vector<UsermodeRecord>::iterator it = response_data.usermodes.begin();
        while(it != response_data.usermodes.end()) {
            time_t time;
            UsermodeRecord u = *it;
            std::ostringstream ss;
            ss << "LISTUSERMODE ";
            ss << "\\usermodeid\\" << u.usermodeid;
            ss << "\\chanmask\\" << u.chanmask;
            ss << "\\modeflags\\" << modeFlagsToModeString(u.modeflags);

            if(u.hostmask.length() != 0)
                ss << "\\hostmask\\" << u.hostmask;
            if(u.machineid.length() != 0)
                ss << "\\machineid\\" << u.machineid;
            if(u.profileid != 0)
                ss << "\\profileid\\" << u.profileid;
            ss << "\\isGlobal\\" << u.isGlobal;

            if(u.expires_at.tv_sec != 0) {
                struct tm * timeinfo;
                time = u.expires_at.tv_sec;
                timeinfo = localtime ( &time );
                if (timeinfo != NULL) {
                    strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
                    ss << "\\expires\\" << timestr;
                }
            }

            if(u.setByUserSummary.nick.length() != 0)
                ss << "\\setbynick\\" << u.setByUserSummary.nick;
            if(u.setByUserSummary.profileid != 0)
                ss << "\\setbypid\\" << u.setByUserSummary.profileid;
            if(u.setByUserSummary.hostname.length() != 0)
                ss << "\\setbyhost\\" << u.setByUserSummary.hostname;
            
            if(u.comment.length() != 0)
                ss << "\\comment\\" << u.comment;

            if(u.set_at.tv_sec != 0) {
                time = u.set_at.tv_sec;
                struct tm * timeinfo;
                timeinfo = localtime ( &time );   
                if (timeinfo != NULL) {
                    strftime(timestr, sizeof(timestr), "%m/%d/%Y %H:%M", timeinfo);
                    ss << "\\setondate\\" << timestr;
                }

            }
                
            ((Peer *)peer)->send_message("PRIVMSG", ss.str(), "SERVER!SERVER@*", ((Peer *)peer)->m_user_details.nick);
            it++;
        }

        if(response_data.is_end) {
            ((Peer *)peer)->send_message("PRIVMSG", "LISTUSERMODE \\final\\1", "SERVER!SERVER@*", ((Peer *)peer)->m_user_details.nick);
        }

        //sendToClient(":SERVER!SERVER@* PRIVMSG %s :LISTUSERMODE \\final\\1",nick);
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