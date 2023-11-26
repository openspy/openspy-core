#include "tasks.h"
#include <uv.h>


#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include <OS/OpenSpy.h>

//#define AMQP_DEBUG_MESSAGES 1

namespace TaskShared {
	uv_once_t mm_tls_init_once = UV_ONCE_INIT;
	uv_key_t mm_redis_connection_key;
	uv_key_t mm_redis_ssl_ctx_key;
	uv_key_t mm_amqp_connection_key;

	void do_init_thread_tls_key() {
		uv_key_create(&mm_redis_connection_key);
		uv_key_create(&mm_amqp_connection_key);
		uv_key_create(&mm_redis_ssl_ctx_key);
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
			redisReply *reply = (redisReply *)redisCommand(connection, "PING");
			if(reply) {
				freeReplyObject(reply);
			}
			if(reply == NULL || connection->err != 0) {
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

		int use_ssl = 0;
		temp_env_sz = sizeof(port_buffer);
		if(uv_os_getenv("OPENSPY_REDIS_SSL", (char *)&port_buffer, &temp_env_sz) == UV_ENOENT) {
		} else {
			use_ssl = atoi(port_buffer);
		}

		redisSSLContext *ssl_context = NULL;
		if(use_ssl) {
			redisSSLContextError ssl_error = REDIS_SSL_CTX_NONE;
			redisInitOpenSSL();

			/* Create SSL context */
			ssl_context = redisCreateSSLContext(
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				&ssl_error);

			if(ssl_context == NULL || ssl_error != REDIS_SSL_CTX_NONE) {
				    OS::LogText(OS::ELogLevel_Error, "hiredis SSL error: %s\n", (ssl_error != REDIS_SSL_CTX_NONE) ? redisSSLContextGetError(ssl_error) : "Unknown error");
			}

		}


		redisOptions redis_options = {0};
		uint16_t port = atoi(port_buffer);
		REDIS_OPTIONS_SET_TCP(&redis_options, address_buffer, port);
		connection = redisConnectWithOptions(&redis_options);

		if(ssl_context == NULL) {
			/* Negotiate SSL/TLS */
			if (redisInitiateSSLWithContext(connection, ssl_context) != REDIS_OK) {
				/* Handle error, in c->err / c->errstr */
				OS::LogText(OS::ELogLevel_Error, "hiredis SSL init error: %s", connection->errstr);
			}
		}

		doRedisAuth(connection);

		uv_key_set(&mm_redis_connection_key, connection);
		uv_key_set(&mm_redis_ssl_ctx_key, ssl_context);

		return connection;
	}
bool print_amqp_error(amqp_rpc_reply_t x, char const *context) {
  switch (x.reply_type) {
    case AMQP_RESPONSE_NORMAL:
      return false;

    case AMQP_RESPONSE_NONE:
      OS::LogText(OS::ELogLevel_Error, "%s: missing RPC reply type!\n", context);
      break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
      OS::LogText(OS::ELogLevel_Error, "%s: %s\n", context, amqp_error_string2(x.library_error));
      break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
      switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
          amqp_connection_close_t *m =
              (amqp_connection_close_t *)x.reply.decoded;
          OS::LogText(OS::ELogLevel_Error, "%s: server connection error %uh, message: %.*s\n",
                  context, m->reply_code, (int)m->reply_text.len,
                  (char *)m->reply_text.bytes);
          break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
          amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
          OS::LogText(OS::ELogLevel_Error, "%s: server channel error %uh, message: %.*s\n",
                  context, m->reply_code, (int)m->reply_text.len,
                  (char *)m->reply_text.bytes);
          break;
        }
        default:
          OS::LogText(OS::ELogLevel_Error, "%s: unknown server error, method id 0x%08X\n",
                  context, x.reply.id);
          break;
      }
      break;
  }
  return true;
}

bool print_amqp_error(int x, char const *context) {
	amqp_rpc_reply_t r;
	r.reply_type = (amqp_response_type_enum_)x;
	return print_amqp_error(r, context);
}

void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody, const OS::Address *peer_address) {
	amqp_connection_state_t connection = getThreadLocalAmqpConnection();

	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_HEADERS_FLAG;
	props.content_type = amqp_cstring_bytes("text/plain");
	props.delivery_mode = 1;


	const int NUM_ENTRIES = 5;
	amqp_table_entry_t entries[NUM_ENTRIES];
	entries[0].key = amqp_cstring_bytes("X-OS-Application");
	entries[0].value.value.bytes = amqp_cstring_bytes(OS::g_appName);
	entries[0].value.kind = AMQP_FIELD_KIND_UTF8;


	entries[1].key = amqp_cstring_bytes("X-OS-Hostname");
	entries[1].value.value.bytes = amqp_cstring_bytes(OS::g_hostName);
	entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
        
    //these 2 are here for compatibility (they are neede by the natneg helper)
    entries[2].key = amqp_cstring_bytes("appName");
    entries[2].value.value.bytes = amqp_cstring_bytes(OS::g_appName);
    entries[2].value.kind = AMQP_FIELD_KIND_UTF8;

    entries[3].key = amqp_cstring_bytes("hostname");
    entries[3].value.value.bytes = amqp_cstring_bytes(OS::g_hostName);
    entries[3].value.kind = AMQP_FIELD_KIND_UTF8;
    
    if(peer_address != NULL) {
        std::string addr = peer_address->ToString();
        entries[4].key = amqp_cstring_bytes("X-OS-Peer-Address");
        entries[4].value.value.bytes = amqp_cstring_bytes(addr.c_str());
        entries[4].value.kind = AMQP_FIELD_KIND_UTF8;
        props.headers.num_entries = NUM_ENTRIES;
    } else {
        props.headers.num_entries = NUM_ENTRIES - 1;
    }
	props.headers.entries = (amqp_table_entry_t*)&entries;
    
    #if AMQP_DEBUG_MESSAGES
    OS::LogText(OS::ELogLevel_Debug, "MQ: [%s,%s] Send Message: %s\n", exchange, routingkey, messagebody);
    #endif //AMQP_DEBUG_MESSAGES


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
		print_amqp_error(amqp_login(listener->amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer), "Login");
        amqp_channel_open_ok_t *channel_open = amqp_channel_open(listener->amqp_listener_conn, 1);
        if(channel_open) {
            uv_thread_create(&listener->amqp_authevent_consumer_thread, TaskShared::amqp_listenerargs_consume_thread, listener);
        } else {
            OS::LogText(OS::ELogLevel_Error, "error opening async AMQP channel");
        }
		
	}

	void amqp_listenerargs_consume_thread(void *arg) {
		const ListenerArgs *listener_args = (ListenerArgs *)arg;
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;		
       
        amqp_basic_consume_ok_t *consume;
        amqp_queue_bind_ok_t *bind_response;
        
        amqp_bytes_t queuename;
        queuename.bytes = NULL;

		amqp_queue_declare_ok_t *queue = amqp_queue_declare(
        	listener_args->amqp_listener_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);

        bool listener_active = false;
        if(queue) {
            OS::LogText(OS::ELogLevel_Error, "Listener queue for: %s", queue->queue.bytes);

			for(int i=0;i<listener_args->num_event_handlers;i++) {
				bind_response = amqp_queue_bind(listener_args->amqp_listener_conn, 1, queue->queue, amqp_cstring_bytes(listener_args->event_handlers[i].amqp_exchange),
						amqp_cstring_bytes(listener_args->event_handlers[i].amqp_routing_key), amqp_empty_table);
				
				if(bind_response) {
					consume = amqp_basic_consume(listener_args->amqp_listener_conn, 1, queue->queue, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
					if(consume) {
						OS::LogText(OS::ELogLevel_Error, "Consumer for (%s,%s): %s", listener_args->event_handlers[i].amqp_exchange, listener_args->event_handlers[i].amqp_routing_key, consume->consumer_tag.bytes);
						listener_active = true;
					} else {
						OS::LogText(OS::ELogLevel_Error, "Failed to consume queue");
					}
				} else {
					OS::LogText(OS::ELogLevel_Error, "Failed to bind queue: %s", queue->queue.bytes);
					//amqp_queue_delete(listener_args->amqp_listener_conn, 1, queue->queue, 0, 0); -- This hangs for some reason
				}
			}
        } else {
            OS::LogText(OS::ELogLevel_Error, "Failed to declare queue");
        }
        
        queuename = amqp_bytes_malloc_dup(queue->queue);
        if (queuename.bytes == NULL) {
            OS::LogText(OS::ELogLevel_Error, "OOM - Failed to duplicate queuename");
            listener_active = false;
        }

		while(listener_active) {
            amqp_maybe_release_buffers(listener_args->amqp_listener_conn);
			res = amqp_consume_message(listener_args->amqp_listener_conn, &envelope, NULL, 0);

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				print_amqp_error(res, "consume message");
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			std::string exchange = std::string((const char *)envelope.exchange.bytes, envelope.exchange.len);
			std::string routingkey = std::string((const char *)envelope.routing_key.bytes, envelope.routing_key.len);

			#if AMQP_DEBUG_MESSAGES
			OS::LogText(OS::ELogLevel_Debug, "MQ: [%s] Delivery %u, exchange %s routingkey %s\n",
                    queuename.bytes,
					(unsigned)envelope.delivery_tag, exchange.c_str(), routingkey.c_str());

			if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
				OS::LogText(OS::ELogLevel_Debug, "MQ: [%s] Content-type: %.*s\n",
                    queuename.bytes,
					(int)envelope.message.properties.content_type.len,
					(char *)envelope.message.properties.content_type.bytes);
			}

			OS::LogText(OS::ELogLevel_Debug, "MQ: [%s] Message: %.*s\n",
                queuename.bytes,
				(int)envelope.message.body.len,
				(char *)envelope.message.body.bytes);
			#endif //AMQP_DEBUG_MESSAGES

			TaskThreadData thread_data;
			thread_data.mp_redis_connection = getThreadLocalRedisContext();

			for(int i=0;i<listener_args->num_event_handlers;i++) {
				if(routingkey.compare(listener_args->event_handlers[i].amqp_routing_key) == 0 && exchange.compare(listener_args->event_handlers[i].amqp_exchange) == 0) {
					listener_args->event_handlers[i].amqp_event_callback(&thread_data, message);
					break;
				}
			}

			amqp_destroy_envelope(&envelope);
		}
        
        if(queuename.bytes != NULL) {
            amqp_bytes_free(queuename);
        }
        if(listener_args->amqp_listener_conn) {
            amqp_maybe_release_buffers(listener_args->amqp_listener_conn);
            amqp_channel_close(listener_args->amqp_listener_conn, 1, AMQP_REPLY_SUCCESS);
            amqp_connection_close(listener_args->amqp_listener_conn, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(listener_args->amqp_listener_conn);
        }
	}

}
