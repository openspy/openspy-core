#include "tasks.h"
namespace Peerchat {
        const char *peerchat_channel_exchange = "peerchat.core";
        const char *peerchat_client_message_routingkey = "peerchat.events";
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            printf("tasks init\n");
            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server);

            scheduler->AddRequestHandler(EPeerchatRequestType_ReserveNickname, Perform_ReserveNickname);

			//scheduler->AddRequestListener("openspy.core", "peerchat.events", Handle_AuthEvent);
			scheduler->DeclareReady();

            return scheduler;
        }
}