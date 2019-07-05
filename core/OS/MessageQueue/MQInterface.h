#ifndef _MQINTERFACE_H
#define _MQINTERFACE_H
#include <string>
namespace MQ {
    typedef void (*_MQMessageHandler)(std::string exchange, std::string routingKey, std::string message, void *extra);
    class IMQInterface {
        public:
            IMQInterface() { };
            virtual ~IMQInterface() { };
            virtual void sendMessage(std::string exchange, std::string routingKey, std::string message) = 0;
            virtual void setReceiver(std::string exchange, std::string routingKey, _MQMessageHandler handler, std::string queueName = "", void *extra = NULL) = 0;
            virtual void deleteReciever(std::string exchange, std::string routingKey, std::string queueName = "") = 0;
            virtual void declareReady() = 0;
			virtual IMQInterface *clone() = 0;
    };
}
#endif //_MQINTERFACE_H