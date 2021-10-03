#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
//#include "../server/QRPeer.h"

#include "tasks.h"


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "qr.message";
	const char *mm_server_event_routingkey = "server.event";
	const char *mm_server_client_acks_routingkey = "client-messages.acks";

    const char *mp_pk_name = "QRID";

	TaskScheduler<MMPushRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		{EMMPushRequestType_GetGameInfoByGameName, PerformGetGameInfo},
		{EMMPushRequestType_Heartbeat, PerformHeartbeat},
		{EMMPushRequestType_Heartbeat_ClearExistingKeys, PerformHeartbeat},
		{EMMPushRequestType_ValidateServer, PerformValidate},
		{EMMPushRequestType_Keepalive, PerformKeepalive},
		{EMMPushRequestType_ClientMessageAck, PerformClientMessageAck},
		{-1, NULL}
	};

	TaskScheduler<MMPushRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		{mm_channel_exchange, mm_client_message_routingkey, Handle_QRMessage},
		{NULL, NULL, NULL}
	};

    TaskScheduler<MMPushRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<MMPushRequest, TaskThreadData> *scheduler = new TaskScheduler<MMPushRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

        scheduler->SetThreadDataFactory(TaskScheduler<MMPushRequest, TaskThreadData>::DefaultThreadDataFactory);
		
		scheduler->DeclareReady();

        return scheduler;
    }

}