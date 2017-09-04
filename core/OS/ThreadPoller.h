#ifndef _OS_THREADPOLLER_H
#define _OS_THREADPOLLER_H
namespace OS {
    class CThreadPoller {

    public:
        CThreadPoller() { };
        virtual ~CThreadPoller() { };
		virtual bool wait() = 0;
		virtual void signal() = 0;
    };
}
#endif //_OS_THREADPOLLER_H