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
    void Peer::handle_channel_join_events(ChannelSummary channel) {
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)GetDriver()->getServer())->GetPeerchatTask();
        PeerchatBackendRequest req;

        req.channel_summary = channel;
        req.channel_modify.set_mode_flags = GetChannelFlags(channel.channel_id);
        req.type = EPeerchatRequestType_UserJoinEvents;
        req.peer = this;
        req.peer->IncRef();
        req.callback = NULL;
        scheduler->AddRequest(req.type, req);
    }
	void Peer::OnJoinChannel(TaskResponse response_data, Peer* peer) {
        if (response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            peer->mp_mutex->lock();
            peer->m_channel_flags[response_data.channel_summary.channel_id] |= EUserChannelFlag_IsInChannel;
            peer->mp_mutex->unlock();
        }

	}
    void Peer::handle_join(std::vector<std::string> data_parser) {
        std::string target = data_parser.at(1);

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;

        if (data_parser.size() > 2) {
            req.channel_summary.password = data_parser.at(2);
        }

        if (GetOperFlags() & OPERPRIVS_INVISIBLE) {
            if (data_parser.size() > 3) { //desired user mode flags
                std::string mode_string = data_parser.at(3);
                for (int i = 0; i < num_user_join_chan_flags; i++) {
                    if (mode_string.find(user_join_chan_flag_map[i].character) != std::string::npos) {
                        req.channel_modify.set_mode_flags |= user_join_chan_flag_map[i].flag;
                    }
                }
            }
        }

        req.type = EPeerchatRequestType_UserJoinChannel;
        req.peer = this;
        req.channel_summary.channel_name = target;
        req.summary = GetUserDetails();
        
        req.peer->IncRef();
        req.callback = OnJoinChannel;
        scheduler->AddRequest(req.type, req);
        
    }
}