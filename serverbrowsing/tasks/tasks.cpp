#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
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


    bool PerformGetServers(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformGetGroups(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformGetServerByKey(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformGetServerByIP(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformSubmitData(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformGetGameInfoByGameName(MMQueryRequest request, TaskThreadData  *thread_data);
    bool PerformGetGameInfoPairByGameName(MMQueryRequest request, TaskThreadData  *thread_data);

    //server update functions
    bool PerformDeleteMissingKeysAndUpdateChanged(MMQueryRequest request, TaskThreadData  *thread_data);

    bool Handle_ServerEventMsg(TaskThreadData *thread_data, std::map<std::string, std::string> kv_data);

    TaskScheduler<MMQueryRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<MMQueryRequest, TaskThreadData> *scheduler = new TaskScheduler<MMQueryRequest, TaskThreadData>(4, server);

        scheduler->SetThreadDataFactory(TaskScheduler<MMQueryRequest, TaskThreadData>::DefaultThreadDataFactory);

        scheduler->AddRequestHandler(EMMQueryRequestType_GetServers, PerformGetServers);
        scheduler->AddRequestHandler(EMMQueryRequestType_GetGroups, PerformGetGroups);
        scheduler->AddRequestHandler(EMMQueryRequestType_GetServerByKey, PerformGetServerByKey);
        scheduler->AddRequestHandler(EMMQueryRequestType_GetServerByIP, PerformGetServerByIP);
        scheduler->AddRequestHandler(EMMQueryRequestType_SubmitData, PerformSubmitData);
        scheduler->AddRequestHandler(EMMQueryRequestType_GetGameInfoByGameName, PerformGetGameInfoByGameName);
        scheduler->AddRequestHandler(EMMQueryRequestType_GetGameInfoPairByGameName, PerformGetGameInfoPairByGameName);
        scheduler->AddRequestListener(mm_channel_exchange, mm_server_event_routingkey, Handle_ServerEventMsg);

		scheduler->DeclareReady();

        return scheduler;
    }

}