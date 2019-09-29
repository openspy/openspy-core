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
    void Peer::handle_join(std::vector<std::string> data_parser) {
        std::string target = data_parser.at(1);
        //JOIN #aaaa


        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_UserJoinChannel;
        req.peer = this;
        req.channel_summary.channel_name = target;
        
        req.peer->IncRef();
        req.callback = NULL;
        scheduler->AddRequest(req.type, req);
        
    }
}