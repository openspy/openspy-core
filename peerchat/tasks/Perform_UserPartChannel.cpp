#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
    bool Perform_UserPartChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
        RemoveUserFromChannel(thread_data, request.peer->GetUserDetails(), channel, "PART", request.message);
		TaskResponse response;
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}