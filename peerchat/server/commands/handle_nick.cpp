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
    void Peer::OnNickReserve(TaskResponse response_data, Peer *peer) {
        if(response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            if(peer->m_sent_client_init) {
                Server *server = (Server *)peer->mp_driver->getServer();
                server->SendUserMessageToVisibleUsers(peer->GetUserDetails().ToString(), "NICK", response_data.profile.uniquenick);
                peer->m_user_details.nick = response_data.profile.uniquenick;
            } else {
                peer->m_user_details.nick = response_data.profile.uniquenick;
                peer->OnUserMaybeRegistered();
            }
            
        } else {
            peer->send_numeric(433,"Nickname is already in use", false, response_data.profile.uniquenick);
        }
    }
    void Peer::handle_nick(std::vector<std::string> data_parser) {
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetUserDetails;
        req.peer = this;
        req.profile.uniquenick = data_parser.at(1);
        req.peer->IncRef();
        req.callback = OnNickReserve;
        scheduler->AddRequest(req.type, req);
    }
}