#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include "../server/QRPeer.h"
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>
#define NATNEG_COOKIE_TIME 30 


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "client.message";
	const char *mm_server_event_routingkey = "server.event";

    const char *mp_pk_name = "QRID";


    bool PerformPushServer(MMPushRequest request, TaskThreadData  *thread_data);
    bool PerformUpdateServer(MMPushRequest request, TaskThreadData  *thread_data);
    bool PerformDeleteServer(MMPushRequest request, TaskThreadData  *thread_data);
    bool PerformGetGameInfo(MMPushRequest request, TaskThreadData  *thread_data);

    //server update functions
    bool PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request, TaskThreadData  *thread_data);

    bool Handle_ClientMessage(TaskThreadData *thread_data, std::map<std::string, std::string> kv_data);

    TaskScheduler<MMPushRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<MMPushRequest, TaskThreadData> *scheduler = new TaskScheduler<MMPushRequest, TaskThreadData>(4, server);

        scheduler->SetThreadDataFactory(TaskScheduler<MMPushRequest, TaskThreadData>::DefaultThreadDataFactory);

        scheduler->AddRequestHandler(EMMPushRequestType_PushServer, PerformPushServer);
        scheduler->AddRequestHandler(EMMPushRequestType_UpdateServer, PerformUpdateServer);
        scheduler->AddRequestHandler(EMMPushRequestType_UpdateServer_NoDiff, PerformDeleteMissingKeysAndUpdateChanged);
        scheduler->AddRequestHandler(EMMPushRequestType_DeleteServer, PerformDeleteServer);
        scheduler->AddRequestHandler(EMMPushRequestType_GetGameInfoByGameName, PerformGetGameInfo);
        scheduler->AddRequestListener(mm_channel_exchange, mm_client_message_routingkey, Handle_ClientMessage);

		scheduler->DeclareReady();

        return scheduler;
    }

}