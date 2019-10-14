#include "tasks.h"
namespace Peerchat {
        const char *peerchat_channel_exchange = "openspy.core";
        const char *peerchat_client_message_routingkey = "peerchat.client-messages";
        const char *peerchat_channel_message_routingkey = "peerchat.channel-messages";
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server);
            scheduler->AddRequestHandler(EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails);
            scheduler->AddRequestHandler(EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget);
            scheduler->AddRequestHandler(EPeerchatRequestType_UserJoinChannel, Perform_UserJoinChannel);
            scheduler->AddRequestHandler(EPeerchatRequestType_LookupChannelDetails, Perform_LookupChannelDetails);

			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_Message);
            scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_channel_message_routingkey, Handle_ChannelMessage);
			scheduler->DeclareReady();

            return scheduler;
        }
}