//#include "../server/QRPeer.h"

#include "tasks.h"

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "qr.message";
	const char *mm_server_event_routingkey = "server.event";
	const char *mm_server_client_acks_routingkey = "client-messages.acks";

	const char *mp_pk_name = "QRID";

	TaskShared::ListenerEventHandler qrmsgs_event_handler = {mm_channel_exchange, mm_client_message_routingkey, Handle_QRMessage}; //this event is sent from the "qr-service" project which handles requeueing and other logic
	TaskShared::ListenerEventHandler all_events[] = {qrmsgs_event_handler};
	TaskShared::ListenerArgs consume_qrmsgs_message = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		if(temp_data.mp_redis_connection == NULL) {
			return;
		}
		
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->request.type) {
			case EMMPushRequestType_GetGameInfoByGameName:
				PerformGetGameInfo(work_data->request, &temp_data);
			break;
			case EMMPushRequestType_Heartbeat:
			case EMMPushRequestType_Heartbeat_ClearExistingKeys:
				PerformHeartbeat(work_data->request, &temp_data);
			break;
			case EMMPushRequestType_ValidateServer:
				PerformValidate(work_data->request, &temp_data);
			break;
			case EMMPushRequestType_Keepalive:
				PerformKeepalive(work_data->request, &temp_data);
			break;
			case EMMPushRequestType_ClientMessageAck:
				PerformClientMessageAck(work_data->request, &temp_data);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}


	void InitTasks() {
		setup_listener(&consume_qrmsgs_message);
	}
}
