#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>

#include <sstream>

#include "tasks.h"

namespace NN {
    const char *nn_channel_exchange = "openspy.natneg", *nn_channel_routingkey="natneg.core";

	TaskScheduler<NNRequestData, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		{ENNRequestType_SubmitJson, PerformSubmitJson},
		{-1, NULL}
	};

	TaskScheduler<NNRequestData, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		{nn_channel_exchange, nn_channel_routingkey, Handle_HandleRecvMessage},
		{NULL, NULL, NULL}
	};

    TaskScheduler<NNRequestData, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<NNRequestData, TaskThreadData> *scheduler = new TaskScheduler<NNRequestData, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

        scheduler->SetThreadDataFactory(TaskScheduler<NNRequestData, TaskThreadData>::DefaultThreadDataFactory);

		scheduler->DeclareReady();

        return scheduler;
    }
}