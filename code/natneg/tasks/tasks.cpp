#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include <sstream>

#include "tasks.h"

#include <OS/tasks.h>
namespace NN {
    const char *nn_channel_exchange = "openspy.natneg", *nn_channel_routingkey="natneg.core";

	TaskShared::ListenerEventHandler nn_msg_handler = {nn_channel_exchange, nn_channel_routingkey, Handle_HandleRecvMessage};
	TaskShared::ListenerEventHandler all_events[] = {nn_msg_handler};
	TaskShared::ListenerArgs consume_nn_message = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};

    void InitTasks() {
		setup_listener(&consume_nn_message);
    }


	void PerformUVWorkRequest(uv_work_t *req) {
		NNRequestData *request = (NNRequestData *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(request->type) {
			case ENNRequestType_SubmitJson:
				PerformSubmitJson(*request);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		NNRequestData *work_data = (NNRequestData *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}
}