#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>

#include "tasks.h"


namespace MM {
	const char *mm_channel_exchange = "openspy.master";
	const char *mm_server_event_routingkey = "server.event";
    const char *mp_pk_name = "QRID";

	TaskScheduler<UTMasterRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		{UTMasterRequestType_Heartbeat, PerformHeartbeat},
		{UTMasterRequestType_ListServers, PerformListServers},
		{UTMasterRequestType_DeleteServer, PerformDeleteServer},
		{NULL, NULL}
	};

	TaskScheduler<UTMasterRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		{NULL, NULL, NULL}
	};

    TaskScheduler<UTMasterRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<UTMasterRequest, TaskThreadData> *scheduler = new TaskScheduler<UTMasterRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

        scheduler->SetThreadDataFactory(TaskScheduler<UTMasterRequest, TaskThreadData>::DefaultThreadDataFactory);

		scheduler->DeclareReady();

        return scheduler;
    }

}