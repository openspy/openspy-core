#include "tasks.h"

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "client.message";
	const char *mm_server_event_routingkey = "server.event";

    const char *mp_pk_name = "QRID";

	uv_thread_t m_amqp_consumer_thread;

	amqp_connection_state_t m_amqp_listener_conn = NULL;

	TaskShared::ListenerEventHandler server_event_handler = {mm_channel_exchange, mm_server_event_routingkey, Handle_ServerEventMsg};
	TaskShared::ListenerEventHandler all_events[] = {server_event_handler};
	TaskShared::ListenerArgs server_event_listener = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};


	void InitTasks() {
		TaskShared::setup_listener(&server_event_listener);
	}

	void FreeServerListQuery(MM::ServerListQuery *query) {
		std::vector<Server *>::iterator it = query->list.begin();
		while(it != query->list.end()) {
			Server *server = *it;
			delete server;
			it++;
		}
	}

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->request.type) {
			case EMMQueryRequestType_GetServers:
				PerformGetServers(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_GetGroups:
				PerformGetGroups(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_GetServerByKey:
				PerformGetServerByKey(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_GetServerByIP:
				PerformGetServerByIP(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_SubmitData:
				PerformSubmitData(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_GetGameInfoByGameName:
				PerformGetGameInfoByGameName(work_data->request, &temp_data);
			break;
			case EMMQueryRequestType_GetGameInfoPairByGameName:
				PerformGetGameInfoPairByGameName(work_data->request, &temp_data);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}

}