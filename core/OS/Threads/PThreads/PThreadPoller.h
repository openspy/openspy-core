#ifndef _CPTHREADPOLLER_H
#define _CPTHREADPOLLER_H
#include <pthread.h>
#include <OS/ThreadPoller.h>
namespace OS {
	class CPThreadPoller : public CThreadPoller {
	public:
		CPThreadPoller();
		~CPThreadPoller();
		bool wait(uint64_t time_ms = 0);
		void signal();
	private:
        pthread_mutex_t m_mutex;
        pthread_cond_t 	m_condition;
		uint32_t m_signal_count;
	};
}
#endif //_CWIN32MUTEX_H
