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


	void amqp_do_consume(void *arg) {
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;		
		
		amqp_bytes_t queuename;

		amqp_queue_declare_ok_t *r = amqp_queue_declare(
        	m_amqp_listener_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);

		queuename = amqp_bytes_malloc_dup(r->queue);

  		amqp_queue_bind(m_amqp_listener_conn, 1, queuename, amqp_cstring_bytes(mm_channel_exchange),
                  amqp_cstring_bytes(mm_server_event_routingkey), amqp_empty_table);

		for(;;) {
			res = amqp_consume_message(m_amqp_listener_conn, &envelope, NULL, 0);

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			Handle_ServerEventMsg(message);

			amqp_destroy_envelope(&envelope);
		}

		amqp_channel_close(m_amqp_listener_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(m_amqp_listener_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(m_amqp_listener_conn);
		m_amqp_listener_conn = NULL;

	}


	void InitTasks() {
		m_amqp_listener_conn = amqp_new_connection();
		amqp_socket_t *amqp_socket = amqp_tcp_socket_new(m_amqp_listener_conn);

		char address_buffer[32];
		char port_buffer[32];

		char user_buffer[32];
		char pass_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);


		uv_os_getenv("OPENSPY_AMQP_ADDRESS", (char *)&address_buffer, &temp_env_sz);

		temp_env_sz = sizeof(port_buffer);
		uv_os_getenv("OPENSPY_AMQP_PORT", (char *)&port_buffer, &temp_env_sz);
		int status = amqp_socket_open(amqp_socket, address_buffer, atoi(port_buffer));

		temp_env_sz = sizeof(user_buffer);
		uv_os_getenv("OPENSPY_AMQP_USER", (char *)&user_buffer, &temp_env_sz);
		temp_env_sz = sizeof(pass_buffer);
		uv_os_getenv("OPENSPY_AMQP_PASSWORD", (char *)&pass_buffer, &temp_env_sz);

		if(status) {
			perror("error opening amqp listener socket");
		}

		amqp_login(m_amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);

		amqp_channel_open(m_amqp_listener_conn, 1);

		uv_thread_create(&m_amqp_consumer_thread, amqp_do_consume, NULL);

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