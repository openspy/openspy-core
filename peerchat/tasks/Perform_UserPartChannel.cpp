#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {
    bool Perform_UserPartChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
        RemoveUserFromChannel(thread_data, request.peer->GetBackendId(), channel.channel_id, "PART");
		TaskResponse response;
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}