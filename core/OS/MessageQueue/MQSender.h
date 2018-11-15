#ifndef _MQSender_H
#define _MQSender_H
#include <string>
class IMQSender {
    public:
        IMQSender() { };
        ~IMQSender() { };
        virtual void sendMessage(std::string message) = 0;
};
#endif //_MQSender_H