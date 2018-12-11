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
			TaskSchedulerRequestType type;
    };
    extern const char *nn_channel_exchange;
    extern const char *nn_channel_routingkey;

    bool PerformSubmit(NNRequestData, TaskThreadData *);
    bool PerformERTTest(NNRequestData, TaskThreadData *);

    bool Handle_SendMsg(TaskThreadData *, std::string message);
    ConnectionSummary LoadConnectionSummary(Redis::Connection *redis_connection, std::string redis_key);
    TaskScheduler<NNRequestData, TaskThreadData> * InitTasks(INetServer *server);
}
#endif // _NN_TASKS_H
