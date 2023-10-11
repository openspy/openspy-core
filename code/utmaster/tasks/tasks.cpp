#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>

#include "tasks.h"


namespace MM {
	const char *mm_channel_exchange = "openspy.master";
	const char *mm_server_event_routingkey = "server.event";
    const char *mp_pk_name = "QRID";

	uv_once_t mm_tls_init_once = UV_ONCE_INIT;
	uv_key_t mm_redis_connection_key;
	uv_key_t mm_amqp_connection_key;
	uv_thread_t m_amqp_consumer_thread;

	uv_work_t internal_game_load_work;

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
	

    void InitTasks() {
		
		MM::MMWorkData *work_data = new MM::MMWorkData();
		work_data->request.type = UTMasterRequestType_InternalLoadGamename;

		uv_handle_set_data((uv_handle_t*) &internal_game_load_work, work_data);

		uv_queue_work(uv_default_loop(), &internal_game_load_work, MM::PerformUVWorkRequest, MM::PerformUVWorkRequestCleanup);
    }

    void selectQRRedisDB(TaskThreadData *thread_data) {
        void *reply;
        reply = redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
        if(reply) {
            freeReplyObject(reply);
        }
    }

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = getThreadLocalRedisContext();
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->request.type) {
			case UTMasterRequestType_Heartbeat:
				PerformHeartbeat(work_data->request, &temp_data);
			break;
			case UTMasterRequestType_ListServers:
				PerformListServers(work_data->request, &temp_data);
			break;
			case UTMasterRequestType_DeleteServer:
				PerformDeleteServer(work_data->request, &temp_data);
			break;
			case UTMasterRequestType_InternalLoadGamename:
				PerformInternalLoadGameData(work_data->request, &temp_data);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		MM::MMWorkData *work_data = (MM::MMWorkData *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
	}

}
