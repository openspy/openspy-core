#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 300
namespace Peerchat {


    bool Handle_ChannelMessage(TaskThreadData *thread_data, std::string message) {
        OS::KVReader kvReader(message);
        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        int channel_id = 0, user_id = 0;
        ChannelSummary channel_summary;
        UserSummary summary;
        if(kvReader.GetValue("type").compare("join") == 0) {
            channel_id = kvReader.GetValueInt("channel_id");
            user_id = kvReader.GetValueInt("user_id");
            summary = LookupUserById(thread_data, user_id);
            channel_summary = LookupChannelById(thread_data, channel_id);
            server->OnChannelMessage("JOIN", summary.ToString(), channel_summary.channel_name, "");
        }
        return true;
    }

    bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        ChannelSummary channel = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name);
        AddUserToChannel(thread_data, request.peer->GetBackendId(), channel.channel_id);
        return true;
    }
}