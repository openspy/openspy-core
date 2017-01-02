#ifndef _CPTHREAD_H
#define _CPTHREAD_H
#include <pthread.h>
#include <OS/Thread.h>
namespace OS {
	class CPThread : public OS::CThread {
	public:
		CPThread(OS::ThreadEntry *entry, void *param,  bool auto_start = true);
		~CPThread();
		void start();
		void stop();
	private:
		static void *cpthread_thread(void *thread);
		pthread_t m_thread;
		int	thread_num;
		bool m_thread_dead;
	};
}
#endif //_CPTHREAD_H
