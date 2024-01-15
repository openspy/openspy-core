#ifndef _NN_TASKS_H
#define _NN_TASKS_H
#include <OS/OpenSpy.h>
#include <string>
#ifdef LEGACYRABBITMQ
	#include <amqp.h>
#else
	#include <rabbitmq-c/amqp.h>
#endif
#include <server/structs.h>
#include <uv.h>

#include <OS/tasks.h>

#define NN_REDIS_EXPIRE_TIME 500

using namespace TaskShared;

NN::ConnectionSummary LoadConnectionSummary(std::string redis_key);
namespace NN {
    class Server;
    class Peer;
    enum ENNRequestType {
        ENNRequestType_SubmitJson
    };
    class NNRequestData {
        public:
            std::string send_string;
			ENNRequestType type;
    };
    extern const char *nn_channel_exchange;
    extern const char *nn_channel_routingkey;

    bool PerformSubmitJson(NNRequestData);

	void PerformUVWorkRequest(uv_work_t *req);
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status);

    bool Handle_HandleRecvMessage(TaskShared::TaskThreadData *thread_data, std::string message);
    void InitTasks();
}
#endif // _NN_TASKS_H
