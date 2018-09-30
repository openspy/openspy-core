#ifndef _OS_THREADPOLLER_H
#define _OS_THREADPOLLER_H
#include <stdint.h>
namespace OS {
    class CThreadPoller {

    public:
        CThreadPoller() { };
        virtual ~CThreadPoller() { };
		virtual bool wait(uint64_t time_ms = 0) = 0;
		virtual void signal() = 0;
    };
}
#endif //_OS_THREADPOLLER_H