#ifndef _NN_TASKS_H
#define _NN_TASKS_H
#include <string>

#include <server/structs.h>

#include <OS/Task/TaskScheduler.h>


#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>

#define NN_REDIS_EXPIRE_TIME 500

NN::ConnectionSummary LoadConnectionSummary(std::string redis_key);
namespace NN {
    class Server;
    class Peer;
    enum ENNRequestType {
        ENNRequestType_SubmitClient,
        ENNRequestType_PerformERTTest_IPUnsolicited,
        ENNRequestType_PerformERTTest_IPPortUnsolicited
    };
    class NNRequestData {
        public:
            NN::Peer *peer;
            NN::ConnectionSummary summary;
    };
    extern const char *nn_channel_exchange;
    extern const char *nn_channel_routingkey;

    bool PerformSubmit(TaskScheduler<NNRequestData, TaskThreadData> *, TaskSchedulerRequestType, NNRequestData, TaskThreadData *);
    bool PerformERTTest(TaskScheduler<NNRequestData, TaskThreadData> *scheduler, TaskSchedulerRequestType, NNRequestData, TaskThreadData *);

    bool Handle_SendMsg(TaskScheduler<NNRequestData, TaskThreadData> *scheduler, TaskThreadData *, std::map<std::string, std::string> kv_data);
    ConnectionSummary LoadConnectionSummary(Redis::Connection *redis_connection, std::string redis_key);
    TaskScheduler<NNRequestData, TaskThreadData> * InitTasks();
}
#endif // _NN_TASKS_H
