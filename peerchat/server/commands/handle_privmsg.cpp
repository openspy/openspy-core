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
    void Peer::handle_privmsg(std::vector<std::string> data_parser) {
        std::string message = data_parser.at(2);
        std::string target = data_parser.at(1);
        if(message[0] == ':') {
            message = message.substr(1);
        }

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SendMessageToTarget;
        req.peer = this;
        req.profile = m_profile;
        req.user = m_user;
        req.summary = m_user_details;
        req.message = message;
        req.message_type = "PRIVMSG";
        req.message_target = target;
        req.peer->IncRef();
        req.callback = OnNickReserve;
        scheduler->AddRequest(req.type, req);
        
    }
}