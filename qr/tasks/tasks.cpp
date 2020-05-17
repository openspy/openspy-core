#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include "../server/QRPeer.h"

#include "tasks.h"


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "client.message";
	const char *mm_server_event_routingkey = "server.event";

    const char *mp_pk_name = "QRID";

	TaskScheduler<MMPushRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		{EMMPushRequestType_PushServer, PerformPushServer},
		{EMMPushRequestType_UpdateServer, PerformUpdateServer},
		{EMMPushRequestType_UpdateServer_NoDiff, PerformDeleteMissingKeysAndUpdateChanged},
		{EMMPushRequestType_DeleteServer, PerformDeleteServer},
		{EMMPushRequestType_GetGameInfoByGameName, PerformGetGameInfo},
		{NULL, NULL}
	};

	TaskScheduler<MMPushRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		{mm_channel_exchange, mm_server_event_routingkey, Handle_ClientMessage},
		{NULL, NULL, NULL}
	};

    TaskScheduler<MMPushRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<MMPushRequest, TaskThreadData> *scheduler = new TaskScheduler<MMPushRequest, TaskThreadData>(4, server, requestTable, listenerTable);

        scheduler->SetThreadDataFactory(TaskScheduler<MMPushRequest, TaskThreadData>::DefaultThreadDataFactory);
		
		scheduler->DeclareReady();

        return scheduler;
    }

}