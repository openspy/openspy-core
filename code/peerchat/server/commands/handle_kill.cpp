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
	void Peer::OnKillUser(TaskResponse response_data, Peer* peer) {
        if(response_data.summary.id == 0) {
            peer->send_no_such_target_error(response_data.profile.uniquenick);
        }
        
	}
	void Peer::handle_kill(std::vector<std::string> data_parser) {
            std::string target = data_parser.at(1);

            std::string message = data_parser.at(2);

            bool do_combine = false;
            if(message[0] == ':') {
                do_combine = true;
                message = message.substr(1);
            }

            if (do_combine) {
                for (size_t i = 3; i < data_parser.size(); i++) {
                    message = message.append(" ").append(data_parser.at(i));
                }
            }

            std::string reason = "KILLED by " + GetUserDetails().nick + ": " + message;

            TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
            PeerchatBackendRequest req;
            req.type = EPeerchatRequestType_RemoteKill_ByName;
            req.peer = this;
            req.summary.username = target;

            req.usermodeRecord.comment = reason;

            req.peer->IncRef();
            req.callback = OnKillUser;
            scheduler->AddRequest(req.type, req);
	}
}