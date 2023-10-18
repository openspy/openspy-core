#ifndef _TASKS_SHARED_H
#define _TASKS_SHARED_H
#include <string>
#include <hiredis/hiredis.h>
#include <rabbitmq-c/amqp.h>
namespace TaskShared {
	class TaskThreadData {
		public:
			redisContext *mp_redis_connection;
	};

	struct curl_data {
		std::string buffer;
	};

    size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp);

	amqp_connection_state_t getThreadLocalAmqpConnection();
	redisContext *getThreadLocalRedisContext();
	void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody);
	
}
#endif //_TASKS_SHARED_H