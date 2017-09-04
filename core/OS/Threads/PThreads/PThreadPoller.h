#ifndef _CPTHREADPOLLER_H
#define _CPTHREADPOLLER_H
#include <pthread.h>
#include <OS/ThreadPoller.h>
namespace OS {
	class CPThreadPoller : public CThreadPoller {
	public:
		CPThreadPoller();
		~CPThreadPoller();
		bool wait();
		void signal();
	private:
        pthread_mutex_t m_mutex;
        pthread_cond_t 	m_condition;
	};
}
#endif //_CWIN32MUTEX_H
