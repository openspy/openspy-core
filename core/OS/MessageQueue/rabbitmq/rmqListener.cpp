#include "rmqListener.h"
namespace MQ {
    RMQListener::RMQListener(std::string hostname, int port, std::string exchangeName, std::string routingKey, std::string username, std::string password, std::string vhost, _MQMessageHandler handler, std::string queuename) : IMQListener() {
        m_hostname = hostname;
        m_port = port;
        m_exchange_name = exchangeName;
        m_routing_key = routingKey;
        m_username = username;
        m_password = password;
        m_vhost = vhost;
        m_queue_name = queuename;
        setRecieverCallback(handler, this);

		mp_thread = NULL;
		Reconnect();
   
    }
    RMQListener::~RMQListener() {
        if(mp_thread) {
        	mp_thread->SignalExit(true);
            delete mp_thread;
        }
		amqp_channel_close(mp_rabbitmq_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(mp_rabbitmq_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(mp_rabbitmq_conn);
    }
    bool RMQListener::handle_amqp_error(amqp_rpc_reply_t x, char const *context) {
        if(x.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && x.library_error == AMQP_STATUS_TIMEOUT) {
            return true;
        }
        switch (x.reply_type) {
            case AMQP_RESPONSE_NORMAL:
            return false;

            case AMQP_RESPONSE_NONE:
            OS::LogText(OS::ELogLevel_Error, "RMQListener: %s: missing RPC reply type!\n", context);
            break;

            case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            OS::LogText(OS::ELogLevel_Error, "RMQListener: %s: %s\n", context, amqp_error_string2(x.library_error));
            break;

            case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                amqp_connection_close_t *m =
                    (amqp_connection_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "RMQListener: %s: server connection error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "RMQListener: %s: server channel error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                default:
                OS::LogText(OS::ELogLevel_Error, "RMQListener: %s: unknown server error, method id 0x%08X\n",
                        context, x.reply.id);
                break;
            }
            Reconnect();
            return true;
            break;
        }
        return true;
    }
    void RMQListener::Reconnect() {
		if(mp_thread)
            mp_thread->SignalExit(true);
        mp_thread = OS::CreateThread(&RMQListener::ListenThread, this, true);
    }
    void *RMQListener::ListenThread(OS::CThread *thread) {
        RMQListener *listener = (RMQListener *)thread->getParams();
		listener->mp_rabbitmq_conn = amqp_new_connection();
		listener->mp_rabbitmq_socket = amqp_tcp_socket_new(listener->mp_rabbitmq_conn);

        amqp_bytes_t queuename;

        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 5;

		int status = amqp_socket_open_noblock(listener->mp_rabbitmq_socket, listener->m_hostname.c_str(), listener->m_port, &timeout);
        if(status != AMQP_STATUS_OK) {
            OS::LogText(OS::ELogLevel_Error, "Error connecting on sender socket");
        }
		amqp_login(listener->mp_rabbitmq_conn, listener->m_vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
			listener->m_username.c_str(), listener->m_password.c_str());
		amqp_channel_open(listener->mp_rabbitmq_conn, 1);
		listener->handle_amqp_error(amqp_get_rpc_reply(listener->mp_rabbitmq_conn), "channel open"); //TODO: channel error check

        if(listener->m_queue_name.length() > 0) {
             queuename = amqp_cstring_bytes(listener->m_queue_name.c_str());
        } else {
            amqp_queue_declare_ok_t *r = amqp_queue_declare(listener->mp_rabbitmq_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
            listener->handle_amqp_error(amqp_get_rpc_reply(listener->mp_rabbitmq_conn), "Declaring queue");
            queuename = amqp_bytes_malloc_dup(r->queue);
            if (queuename.bytes == NULL) {
                fprintf(stderr, "Out of memory while copying queue name");
                exit(-1);
            }

            amqp_queue_bind(listener->mp_rabbitmq_conn, 1, queuename, amqp_cstring_bytes(listener->m_exchange_name.c_str()),
                            amqp_cstring_bytes(listener->m_routing_key.c_str()), amqp_empty_table);

            listener->handle_amqp_error(amqp_get_rpc_reply(listener->mp_rabbitmq_conn), "Binding queue");
        }


		amqp_basic_consume(listener->mp_rabbitmq_conn, 1, queuename, amqp_empty_bytes, 0, 1, 0,
			amqp_empty_table);

        listener->handle_amqp_error(amqp_get_rpc_reply(listener->mp_rabbitmq_conn), "basic consume");

   		while (thread->isRunning()) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;

			amqp_maybe_release_buffers(listener->mp_rabbitmq_conn);

			res = amqp_consume_message(listener->mp_rabbitmq_conn, &envelope, &timeout, 0);
			
			if(listener->handle_amqp_error(res, "consume message")) {
                continue;
            }

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

            //amqp_basic_ack(listener->mp_rabbitmq_conn, envelope.channel, envelope.delivery_tag, false);

            listener->mp_message_handler(message, listener->mp_extra);

			amqp_destroy_envelope(&envelope);
        }

        amqp_bytes_free(queuename);
		amqp_channel_close(listener->mp_rabbitmq_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(listener->mp_rabbitmq_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(listener->mp_rabbitmq_conn);

        return NULL;
    }
}