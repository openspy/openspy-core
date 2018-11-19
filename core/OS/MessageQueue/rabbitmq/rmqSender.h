#ifndef _RMQSender_H
#define _RMQSender_H
#include "../MQSender.h"
#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
namespace MQ {
    class RMQSender;
	class RMQSenderRequest {
		public:
			RMQSenderRequest(std::string message) {
                m_message = message;
			}
			~RMQSenderRequest() { }
			std::string m_message;
	};
    class RMQSenderTask : public OS::Task<RMQSenderRequest> {
        public:
            RMQSenderTask(RMQSender *sender, std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost);
            ~RMQSenderTask();
        private:
            amqp_socket_t *mp_rabbitmq_socket;
            amqp_connection_state_t mp_rabbitmq_conn;

            
            std::string m_hostname;
            int m_port;
            std::string m_exchange_name;
            std::string m_routing_key;

            std::string m_user;
            std::string m_password;
            std::string m_vhost;
            
            static void *TaskThread(OS::CThread *thread);
            void handle_amqp_error(amqp_rpc_reply_t x, char const *context);
    };
    class RMQSender : public IMQSender {
        public:
            RMQSender(std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost);
            ~RMQSender();
            void sendMessage(std::string message);
        private:
            RMQSenderTask *mp_task;
    };
}
#endif //_MQSender_H