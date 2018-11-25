#include "rmqConnection.h"
#define RMQSENDER_WAIT_MAX_TIME 1500
namespace MQ {

    rmqConnection::rmqConnection(OS::Address address, std::string username, std::string password, std::string vhost) {
        m_address = address;
        m_username = username;
        m_password = password;
        m_vhost = vhost;

        mp_mutex = OS::CreateMutex();
        mp_rabbitmq_socket = NULL;
        mp_reconnect_retry_thread = NULL;

        connect();
    }
    rmqConnection::~rmqConnection() {
        disconnect();

        if(mp_reconnect_retry_thread != NULL) {
            mp_reconnect_retry_thread->SignalExit(true);
            delete mp_reconnect_retry_thread;
        }
    }
    void rmqConnection::sendMessage(std::string exchange, std::string routingKey, std::string message) {
        if(mp_rabbitmq_conn == NULL) {
            reconnect();
            if(mp_rabbitmq_conn != NULL) {
                sendMessage(exchange, routingKey, message);
            }
            return;
        }
        amqp_basic_properties_t props;
                props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
                props.content_type = amqp_cstring_bytes("text/plain");
                props.delivery_mode = 2; // persistent delivery mode
        amqp_basic_publish(mp_rabbitmq_conn, 1, amqp_cstring_bytes(exchange.c_str()),
            amqp_cstring_bytes(routingKey.c_str()), 0, 0,
            &props, amqp_cstring_bytes(message.c_str()));

        handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "basic publish"); //TODO: channel error check
    }
    void rmqConnection::deleteReciever(std::string exchange, std::string routingKey, std::string queueName) {

    }

    void rmqConnection::connect() {
        if(mp_rabbitmq_conn != NULL) 
            disconnect();
		//init sender
		mp_rabbitmq_conn = amqp_new_connection();
		mp_rabbitmq_socket = amqp_tcp_socket_new(mp_rabbitmq_conn);

        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 5;

		int status = amqp_socket_open_noblock(mp_rabbitmq_socket, m_address.ToString(true).c_str(), m_address.GetPort(), &timeout);
        if(status != AMQP_STATUS_OK) {
            OS::LogText(OS::ELogLevel_Error, "Error connecting on sender socket");
            disconnect();
            spawnReconnectThread();
        } else {
		    amqp_login(mp_rabbitmq_conn, m_vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, m_username.c_str(), m_password.c_str());
		    amqp_channel_open(mp_rabbitmq_conn, 1);
    		handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "channel open"); //TODO: channel error check

            mp_listen_thread = OS::CreateThread(rmqConnection::ListenThread, this, true);

        }
    }
    void rmqConnection::spawnReconnectThread() {
        if(!mp_reconnect_retry_thread) {
            mp_reconnect_retry_thread = OS::CreateThread(rmqConnection::ReconnectRetryThread, this, true);
        }
    }
    void rmqConnection::reconnect() {
        disconnect();
        connect();
        if(mp_rabbitmq_conn) {
            setupRecievers();
        }
        
    }

    void rmqConnection::disconnect() {
        if(mp_rabbitmq_conn == NULL) return;
        if(mp_listen_thread) {
            mp_listen_thread->SignalExit(true);
            delete mp_listen_thread;
            mp_listen_thread = NULL;
        }

		amqp_channel_close(mp_rabbitmq_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(mp_rabbitmq_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(mp_rabbitmq_conn);
        mp_rabbitmq_conn = NULL;
    }

    bool rmqConnection::handle_amqp_error(amqp_rpc_reply_t x, char const *context) {
        switch (x.reply_type) {
            case AMQP_RESPONSE_NORMAL:
            return false;

            case AMQP_RESPONSE_NONE:
            OS::LogText(OS::ELogLevel_Error, "rmqConnection: %s: missing RPC reply type!\n", context);
            break;

            case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            OS::LogText(OS::ELogLevel_Error, "rmqConnection: %s: %s\n", context, amqp_error_string2(x.library_error));
            return true;
            break;

            case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                amqp_connection_close_t *m =
                    (amqp_connection_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "rmqConnection: %s: server connection error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
                OS::LogText(OS::ELogLevel_Error, "rmqConnection: %s: server channel error %uh, message: %.*s\n",
                        context, m->reply_code, (int)m->reply_text.len,
                        (char *)m->reply_text.bytes);
                break;
                }
                default:
                OS::LogText(OS::ELogLevel_Error, "rmqConnection: %s: unknown server error, method id 0x%08X\n",
                        context, x.reply.id);
                break;
            }
            reconnect();
            return true;
            break;
        }
        return false;
    }
    void *rmqConnection::ListenThread(OS::CThread *thread) {
        rmqConnection *listener = (rmqConnection *)thread->getParams();

        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 5;

   		while (thread->isRunning()) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;

			amqp_maybe_release_buffers(listener->mp_rabbitmq_conn);

			res = amqp_consume_message(listener->mp_rabbitmq_conn, &envelope, &timeout, 0);
			
			if(listener->handle_amqp_error(res, "consume message")) {
                continue;
            }

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

            std::string routing_key = std::string((char *)envelope.routing_key.bytes);
            listener->mp_mutex->lock();
            std::map<std::string, rmqListenerData *>::iterator it = listener->m_listener_callbacks.find(routing_key);
            rmqListenerData *rmqlistener = NULL;
            if(it != listener->m_listener_callbacks.end()) {
                rmqlistener = (*it).second;
            }
            listener->mp_mutex->unlock();
            
            if(rmqlistener != NULL)
                rmqlistener->handler(message, rmqlistener->extra);            

            //amqp_basic_ack(listener->mp_rabbitmq_conn, envelope.channel, envelope.delivery_tag, false);

			amqp_destroy_envelope(&envelope);
        }
        return NULL;
    }
    void *rmqConnection::ReconnectRetryThread(OS::CThread *thread) {
        rmqConnection *listener = (rmqConnection *)thread->getParams();
        while(listener->mp_rabbitmq_conn == NULL) {
            OS::Sleep(5000);
            listener->reconnect();            
        }
        delete listener->mp_reconnect_retry_thread; //safe to delete within own thread?? -- should add "auto cleanup" option to thread class
        listener->mp_reconnect_retry_thread = NULL;
        return NULL;
    }
    void rmqConnection::setReciever(std::string exchange, std::string routingKey, _MQMessageHandler handler, std::string queueName, void *extra) {
        rmqListenerData *entry = new rmqListenerData;
        entry->extra = extra;
        entry->handler = handler;
        entry->exchange = exchange;
        entry->queueName = queueName;
        entry->routingKey = routingKey;
        mp_mutex->lock();
        m_listener_callbacks[routingKey] = entry;
        mp_mutex->unlock();

        if(mp_rabbitmq_conn == NULL) return;

        amqp_bytes_t queuename;
        if(queueName.length() > 0) {
             queuename = amqp_cstring_bytes(queueName.c_str());
        } else {
            amqp_queue_declare_ok_t *r = amqp_queue_declare(mp_rabbitmq_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
            handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "Declaring queue");
            queuename = amqp_bytes_malloc_dup(r->queue);
            if (queuename.bytes == NULL) {
                fprintf(stderr, "Out of memory while copying queue name");
                exit(-1);
            }
        }

        amqp_queue_bind(mp_rabbitmq_conn, 1, queuename, amqp_cstring_bytes(exchange.c_str()),
                        amqp_cstring_bytes(routingKey.c_str()), amqp_empty_table);

        handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "Binding queue");


		amqp_basic_consume(mp_rabbitmq_conn, 1, queuename, amqp_empty_bytes, 0, 1, 0,
			amqp_empty_table);

        handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "basic consume");
    }
    void rmqConnection::setupRecievers() {
        std::map<std::string, rmqListenerData *>::iterator it = m_listener_callbacks.begin();
        while(it != m_listener_callbacks.end()) {
            std::pair<std::string, rmqListenerData *> p = *it;
            rmqListenerData *listenerData = (*it).second;
            amqp_bytes_t queuename;
            if(listenerData->queueName.length() > 0) {
                    queuename = amqp_cstring_bytes(listenerData->queueName.c_str());
            } else {
                amqp_queue_declare_ok_t *r = amqp_queue_declare(mp_rabbitmq_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
                handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "Declaring queue");
                if(!r) {
                    disconnect();
                    return;
                }
                queuename = amqp_bytes_malloc_dup(r->queue);
                if (queuename.bytes == NULL) {
                    fprintf(stderr, "Out of memory while copying queue name");
                    exit(-1);
                }
            }

            amqp_queue_bind(mp_rabbitmq_conn, 1, queuename, amqp_cstring_bytes(listenerData->exchange.c_str()),
                            amqp_cstring_bytes(listenerData->routingKey.c_str()), amqp_empty_table);

            handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "Binding queue");


            amqp_basic_consume(mp_rabbitmq_conn, 1, queuename, amqp_empty_bytes, 0, 1, 0,
                amqp_empty_table);

            handle_amqp_error(amqp_get_rpc_reply(mp_rabbitmq_conn), "basic consume");

            it++;
        }
    }
}