#ifndef _RMQCONNECTION_H
#define _RMQCONNECTION_H
#include "../MQInterface.h"
#include <OS/OpenSpy.h>
#include <OS/Thread.h>
#include <OS/Task.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
namespace MQ {
    class rmqListenerData {
        public:
            void *extra;
            std::string exchange;
            std::string routingKey;
            std::string queueName;
            _MQMessageHandler handler;
    };

    class rmqConnection : public IMQInterface {
        friend class rmqSenderTask;
        public:
            rmqConnection(OS::Address address, std::string username, std::string password, std::string vhost);
            ~rmqConnection();
            void sendMessage(std::string exchange, std::string routingKey, std::string message);
            void setReciever(std::string exchange, std::string routingKey, _MQMessageHandler handler, std::string queueName = "", void *extra = NULL);
            void deleteReciever(std::string exchange, std::string routingKey, std::string queueName = "");
            static void *ListenThread(OS::CThread *thread);
        private:
            void connect();
            void disconnect();
            void reconnect();

            void initConsumers();
            bool handle_amqp_error(amqp_rpc_reply_t x, char const *context);


            amqp_socket_t *mp_rabbitmq_socket;
            amqp_connection_state_t mp_rabbitmq_conn;
            OS::Address m_address;

            std::map<std::string, rmqListenerData *> m_listener_callbacks;

            std::string m_username;
            std::string m_password;
            std::string m_vhost;

            OS::CThread *mp_listen_thread;
            OS::CMutex *mp_mutex;
            
    };
}
#endif //_RMQCONNECTION_H