#include "rmqSender.h"
#define RMQSENDER_WAIT_MAX_TIME 1500
namespace MQ {
    
    RMQSenderTask::RMQSenderTask(RMQSender *sender, std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost) {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(RMQSenderTask::TaskThread, this, true);

        m_hostname = hostname;
        m_port = port;
        m_vhost = vhost;
        m_user = username;
        m_password = password;
        m_exchange_name = exchangeName;
        m_routing_key = routingKey;

		//init sender
		mp_rabbitmq_conn = amqp_new_connection();
		mp_rabbitmq_socket = amqp_tcp_socket_new(mp_rabbitmq_conn);

        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 5;

		int status = amqp_socket_open_noblock(mp_rabbitmq_socket, m_hostname.c_str(), m_port, &timeout);
        if(status != AMQP_STATUS_OK) {
            OS::LogText(OS::ELogLevel_Error, "Error connecting on sender socket");
        }
		amqp_login(mp_rabbitmq_conn, m_vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
			m_user.c_str(), m_password.c_str());
		amqp_channel_open(mp_rabbitmq_conn, 1);
		handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "channel open"); //TODO: channel error check
    }
    RMQSenderTask::~RMQSenderTask() {
        if(mp_thread) {
            mp_thread->SignalExit(true, mp_thread_poller);
            delete mp_thread;
        }
		
        if(mp_mutex) {
		    delete mp_mutex;
        }
    }
    void *RMQSenderTask::TaskThread(OS::CThread *thread) {
		RMQSenderTask *task = (RMQSenderTask *)thread->getParams();
		while (thread->isRunning() && (!task->m_request_list.empty() || task->mp_thread_poller->wait(RMQSENDER_WAIT_MAX_TIME)) && thread->isRunning()) {
			task->mp_mutex->lock();
			while (!task->m_request_list.empty()) {
				RMQSenderRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();

                amqp_basic_properties_t props;
                        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
                        props.content_type = amqp_cstring_bytes("text/plain");
                        props.delivery_mode = 2; // persistent delivery mode
                        amqp_basic_publish(task->mp_rabbitmq_conn, 1, amqp_cstring_bytes(task->m_exchange_name.c_str()),
                            amqp_cstring_bytes(task->m_routing_key.c_str()), 0, 0,
                            &props, amqp_cstring_bytes(task_params.m_message.c_str()));

                        task->handle_amqp_error(amqp_get_rpc_reply(task->mp_rabbitmq_conn), "basic publish"); //TODO: channel error check


				task->mp_mutex->lock();
				task->m_request_list.pop();
			}
			task->mp_mutex->unlock();
		}
        return NULL;
    }
    void RMQSenderTask::handle_amqp_error(amqp_rpc_reply_t x, char const *context) {
        switch (x.reply_type) {
            case AMQP_RESPONSE_NORMAL:
            return;

            case AMQP_RESPONSE_NONE:
            OS::LogText(OS::ELogLevel_Error, "RMQSender: %s: missing RPC reply type!\n", context);
            break;

            case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            OS::LogText(OS::ELogLevel_Error, "RMQSender: %s: %s\n", context, amqp_error_string2(x.library_error));
            break;

            case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                amqp_connection_close_t *m =
                    (amqp_connection_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "RMQSender: %s: server connection error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "RMQSender: %s: server channel error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                default:
                OS::LogText(OS::ELogLevel_Error, "RMQSender: %s: unknown server error, method id 0x%08X\n",
                        context, x.reply.id);
                break;
            }
            break;
        }
    }

    RMQSender::RMQSender(std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost) : IMQSender() {
            mp_task = new RMQSenderTask(this, hostname, port, exchangeName, routingKey, username, password, vhost);

    }
    RMQSender::~RMQSender() {

    }
    void RMQSender::sendMessage(std::string message) {
        mp_task->AddRequest(RMQSenderRequest(message));
    }
}