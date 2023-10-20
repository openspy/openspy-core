#ifndef _TASKS_SHARED_H
#define _TASKS_SHARED_H
#include <string>
#include <hiredis/hiredis.h>
#include <rabbitmq-c/amqp.h>
#include <uv.h>
namespace OS {
	class Address;
}
namespace TaskShared {
	class TaskThreadData {
		public:
			redisContext *mp_redis_connection;
	};

	struct curl_data {
		std::string buffer;
	};

	typedef struct {
		const char *amqp_exchange;
		const char *amqp_routing_key;
		bool (*amqp_event_callback)(TaskThreadData *, std::string);
		uv_thread_t amqp_authevent_consumer_thread;
		amqp_connection_state_t amqp_listener_conn;
		amqp_socket_t *amqp_socket;
	} ListenerArgs;

    size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp);

	amqp_connection_state_t getThreadLocalAmqpConnection();
	redisContext *getThreadLocalRedisContext();
	void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody, const OS::Address *peer_address = NULL);
	void setup_listener(ListenerArgs *args);
	void amqp_listenerargs_consume_thread(void *arg);
	
}
#endif //_TASKS_SHARED_H