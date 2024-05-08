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
    void Peer::OnFetch_LUsers(TaskResponse response_data, Peer* peer) {
        std::ostringstream s;
        s << "There are " << response_data.usermode.usermodeid << " users";
        peer->send_numeric(251, s.str());
    }
    void Peer::handle_lusers(std::vector<std::string> data_parser) {
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_CountServerUsers;
        req.peer = this;
        req.peer->IncRef();
        req.callback = OnFetch_LUsers;
        AddTaskRequest(req);
    }
}