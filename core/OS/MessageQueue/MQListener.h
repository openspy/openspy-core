#ifndef _MQListener_H
#define _MQListener_H
#include <string>
typedef void (*_MQMessageHandler)(std::string message, void *extra);
class IMQListener {
    public:
        IMQListener() { };
        ~IMQListener() { };
        void setRecieverCallback(_MQMessageHandler handler, void *extra = NULL) {
            mp_message_handler = handler;
            mp_extra = extra;
        };
    protected:
        _MQMessageHandler mp_message_handler;
        void *mp_extra;
};

#endif //_MQListener_H