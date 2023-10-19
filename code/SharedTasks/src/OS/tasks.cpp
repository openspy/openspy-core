#include "tasks.h"
#include <uv.h>


#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include <OS/OpenSpy.h>

namespace TaskShared {
	uv_once_t mm_tls_init_once = UV_ONCE_INIT;
	uv_key_t mm_redis_connection_key;
	uv_key_t mm_amqp_connection_key;

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


        if(uv_os_getenv("OPENSPY_AMQP_ADDRESS", (char *)&address_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp address environment variable");
			return NULL;
		}

        temp_env_sz = sizeof(port_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_PORT", (char *)&port_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp port environment variable");
			return NULL;
		}

        temp_env_sz = sizeof(user_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_USER", (char *)&user_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp user environment variable");
			return NULL;
		}
        temp_env_sz = sizeof(pass_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_PASSWORD", (char *)&pass_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp password environment variable");
			return NULL;
		}

        int status = amqp_socket_open(amqp_socket, address_buffer, atoi(port_buffer));

        if(status) {
            OS::LogText(OS::ELogLevel_Error, "error opening amqp listener socket");
			return NULL;
        }

        amqp_login(connection, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);

        amqp_channel_open(connection, 1);

        uv_key_set(&mm_amqp_connection_key, connection);

        return connection;

    }

	void doRedisAuth(redisContext *connection) {
		char username_buffer[32];
		char password_buffer[32];

		size_t temp_env_sz = sizeof(username_buffer);
		if(uv_os_getenv("OPENSPY_REDIS_USER", (char *)&username_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing redis username environment variable");
			return;
		}

		temp_env_sz = sizeof(password_buffer);
		if(uv_os_getenv("OPENSPY_REDIS_PASSWORD", (char *)&password_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing redis password environment variable");
			return;
		}

		void *redisReply = redisCommand(connection, "AUTH %s %s", username_buffer, password_buffer);
		if(redisReply) {
			freeReplyObject(redisReply);
		}
		
	}

	redisContext *doRedisReconnect() {
		redisContext *connection = (redisContext *)uv_key_get(&mm_redis_connection_key);
		int r = redisReconnect(connection);

		if(r != REDIS_OK) {
			return NULL;
		}

		doRedisAuth(connection);

		uv_key_set(&mm_redis_connection_key, connection);
		return connection;
	}

	redisContext *getThreadLocalRedisContext() {
		uv_once(&mm_tls_init_once, do_init_thread_tls_key);

		redisContext *connection = (redisContext *)uv_key_get(&mm_redis_connection_key);

		if(connection != NULL) {
			if(connection->err != 0) {
				return doRedisReconnect();
			}
			return connection;
		}

		char address_buffer[32];
		char port_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);
		if(uv_os_getenv("OPENSPY_REDIS_ADDRESS", (char *)&address_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing redis address environment variable");
			return NULL;
		}
		temp_env_sz = sizeof(port_buffer);
		if(uv_os_getenv("OPENSPY_REDIS_PORT", (char *)&port_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing redis port environment variable");
			return NULL;
		}


		redisOptions redis_options = {0};
		uint16_t port = atoi(port_buffer);
		REDIS_OPTIONS_SET_TCP(&redis_options, address_buffer, port);
		connection = redisConnectWithOptions(&redis_options);

		doRedisAuth(connection);

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

	void setup_listener(ListenerArgs *listener) {
		char address_buffer[32];
		char port_buffer[32];

		char user_buffer[32];
		char pass_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);

        if(uv_os_getenv("OPENSPY_AMQP_ADDRESS", (char *)&address_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp address environment variable");
			return;
		}

        temp_env_sz = sizeof(port_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_PORT", (char *)&port_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp port environment variable");
			return;
		}

        temp_env_sz = sizeof(user_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_USER", (char *)&user_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp user environment variable");
			return;
		}
        temp_env_sz = sizeof(pass_buffer);
        if(uv_os_getenv("OPENSPY_AMQP_PASSWORD", (char *)&pass_buffer, &temp_env_sz) == UV_ENOENT) {
			OS::LogText(OS::ELogLevel_Error, "Missing amqp password environment variable");
			return;
		}



		//setup generic presence listener
		listener->amqp_listener_conn = amqp_new_connection();
		listener->amqp_socket = amqp_tcp_socket_new(listener->amqp_listener_conn);
		int status = amqp_socket_open(listener->amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			OS::LogText(OS::ELogLevel_Error, "error opening amqp listener socket");
		}
		amqp_login(listener->amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(listener->amqp_listener_conn, 1);
		uv_thread_create(&listener->amqp_authevent_consumer_thread, TaskShared::amqp_listenerargs_consume_thread, listener);
	}

	void amqp_listenerargs_consume_thread(void *arg) {
		ListenerArgs *listener_args = (ListenerArgs *)arg;
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;		
		
		amqp_bytes_t queuename;

		amqp_queue_declare_ok_t *r = amqp_queue_declare(
        	listener_args->amqp_listener_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);

		queuename = amqp_bytes_malloc_dup(r->queue);

  		amqp_queue_bind(listener_args->amqp_listener_conn, 1, queuename, amqp_cstring_bytes(listener_args->amqp_exchange),
                  amqp_cstring_bytes(listener_args->amqp_routing_key), amqp_empty_table);

		for(;;) {
			res = amqp_consume_message(listener_args->amqp_listener_conn, &envelope, NULL, 0);

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			listener_args->amqp_event_callback(NULL, message);

			amqp_destroy_envelope(&envelope);
		}

		amqp_channel_close(listener_args->amqp_listener_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(listener_args->amqp_listener_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(listener_args->amqp_listener_conn);
	}

}