#ifndef _NN_TASKS_H
#define _NN_TASKS_H
#include <OS/OpenSpy.h>
#include <string>
#include <rabbitmq-c/amqp.h>
#include <server/structs.h>
#include <uv.h>

#define NN_REDIS_EXPIRE_TIME 500

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

    bool Handle_HandleRecvMessage(std::string message);
    void InitTasks();
}
#endif // _NN_TASKS_H
