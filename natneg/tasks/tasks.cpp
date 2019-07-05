#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>

#include <sstream>

#include "tasks.h"

namespace NN {
    const char *nn_channel_exchange = "openspy.natneg", *nn_channel_routingkey="natneg.core";

    TaskScheduler<NNRequestData, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<NNRequestData, TaskThreadData> *scheduler = new TaskScheduler<NNRequestData, TaskThreadData>(4, server);

        scheduler->SetThreadDataFactory(TaskScheduler<NNRequestData, TaskThreadData>::DefaultThreadDataFactory);

        scheduler->AddRequestHandler(ENNRequestType_SubmitJson, PerformSubmitJson);
        scheduler->AddRequestListener(nn_channel_exchange, nn_channel_routingkey, Handle_HandleRecvMessage);

		scheduler->DeclareReady();

        return scheduler;
    }
}