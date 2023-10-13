#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
//#include "../server/QRPeer.h"

#include "tasks.h"

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>


namespace MM {
	uv_once_t mm_tls_init_once = UV_ONCE_INIT;
	uv_key_t mm_redis_connection_key;
	uv_key_t mm_amqp_connection_key;
	uv_thread_t m_amqp_consumer_thread;

	amqp_connection_state_t m_amqp_listener_conn = NULL;

	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "qr.message";
	const char *mm_server_event_routingkey = "server.event";
	const char *mm_server_client_acks_routingkey = "client-messages.acks";

    const char *mp_pk_name = "QRID";

	void amqp_do_consume(void *arg) {
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;		
		
		amqp_bytes_t queuename;

		amqp_queue_declare_ok_t *r = amqp_queue_declare(
        	m_amqp_listener_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);

		queuename = amqp_bytes_malloc_dup(r->queue);

  		amqp_queue_bind(m_amqp_listener_conn, 1, queuename, amqp_cstring_bytes(mm_channel_exchange),
                  amqp_cstring_bytes(mm_client_message_routingkey), amqp_empty_table);

		for(;;) {
			res = amqp_consume_message(m_amqp_listener_conn, &envelope, NULL, 0);

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			Handle_QRMessage(message);

			amqp_destroy_envelope(&envelope);
		}

		amqp_channel_close(m_amqp_listener_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(m_amqp_listener_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(m_amqp_listener_conn);
		m_amqp_listener_conn = NULL;

	}

	void do_init_thread_tls_key() {
		uv_key_create(&mm_redis_connection_key);
		uv_key_create(&mm_amqp_connection_key);
	}

	amqp_connection_state_t getThreadLocalAmqpConnection() {
		uv_once(&mm_tls_init_once, do_init_thread_tls_key);
		amqp_connection_state_t connection = (amqp_connection_state_t)uv_key_get(&mm_amqp_connection_key);

		if(connection != NULL) {
			return connection;
		}

		char address_buffer[32];
		char port_buffer[32];

		char user_buffer[32];
		char pass_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);

		connection = amqp_new_connection();
		amqp_socket_t *amqp_socket = amqp_tcp_socket_new(connection);


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

		amqp_login(connection, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);

		amqp_channel_open(connection, 1);

		uv_key_set(&mm_amqp_connection_key, connection);

		return connection;

	}

	redisContext *getThreadLocalRedisContext() {
		uv_once(&mm_tls_init_once, do_init_thread_tls_key);

		redisContext *connection = (redisContext *)uv_key_get(&mm_redis_connection_key);

		if(connection != NULL) {
			return connection;
		}

		//XXX: reconnect logic here

		char address_buffer[32];
		char port_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);
		uv_os_getenv("OPENSPY_REDIS_ADDRESS", (char *)&address_buffer, &temp_env_sz);
		temp_env_sz = sizeof(port_buffer);
		uv_os_getenv("OPENSPY_REDIS_PORT", (char *)&port_buffer, &temp_env_sz);

		redisOptions redis_options = {0};
		uint16_t port = atoi(port_buffer);
		REDIS_OPTIONS_SET_TCP(&redis_options, address_buffer, port);
		connection = redisConnectWithOptions(&redis_options);

		void *redisReply = redisCommand(connection, "AUTH %s %s", OS::g_redisUsername, OS::g_redisPassword);
		freeReplyObject(redisReply);

		uv_key_set(&mm_redis_connection_key, connection);

		return connection;
	}

	void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody) {
		amqp_connection_state_t connection = getThreadLocalAmqpConnection();

		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = 1;

		amqp_basic_publish(connection, 1, amqp_cstring_bytes(exchange),
                                    amqp_cstring_bytes(routingkey), 0, 0,
                                    &props, amqp_cstring_bytes(messagebody));

	}

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = getThreadLocalRedisContext();
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
	void work_close_callback(uv_handle_t *handle) {
		free(handle);
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		uv_close((uv_handle_t*)req, work_close_callback);
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
}
