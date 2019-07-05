#include "tasks.h"
namespace Peerchat {
        const char *peerchat_channel_exchange = "openspy.core";
        const char *peerchat_client_message_routingkey = "peerchat.messages";
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server);

            scheduler->AddRequestHandler(EPeerchatRequestType_ReserveNickname, Perform_ReserveNickname);
            scheduler->AddRequestHandler(EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails);
            scheduler->AddRequestHandler(EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget);
            

			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_Message);
			scheduler->DeclareReady();

            return scheduler;
        }
}