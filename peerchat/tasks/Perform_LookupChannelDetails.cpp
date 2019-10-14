#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

    bool Perform_LookupChannelDetails(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        //ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name)

        ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name);
        printf("LOOKUP CHANNEL DETAILS %s - %d\n", request.channel_summary.channel_name.c_str(), summary.users.size());
        return true;
    }
}