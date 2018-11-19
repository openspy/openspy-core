#ifndef _RMQListener_H
#define _RMQListener_H
#include "../MQListener.h"
#include <OS/OpenSpy.h>
#include <OS/Thread.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
namespace MQ {
    class RMQListener : public IMQListener {
        public:
            RMQListener(std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost, _MQMessageHandler handler, std::string queueName = "");
            ~RMQListener();
        private:
            static void *ListenThread(OS::CThread *thread);
            void Reconnect();
            bool handle_amqp_error(amqp_rpc_reply_t x, char const *context);
            amqp_socket_t *mp_rabbitmq_socket;
            amqp_connection_state_t mp_rabbitmq_conn;
            std::string m_hostname;
            int m_port;
            std::string m_exchange_name;
            std::string m_routing_key;

            std::string m_username;
            std::string m_password;
            std::string m_vhost;

            std::string m_queue_name;

            OS::CThread *mp_thread;
    };
}
#endif //_RMQListener_H