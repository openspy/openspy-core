#ifndef _CPMUTEX_H
#define _CPMUTEX_H
#include <pthread.h>
#include <OS/Mutex.h>
namespace OS {
	class CPMutex : public CMutex {
	public:
		CPMutex();
		~CPMutex();
		void lock();
		void unlock();
	private:
		pthread_mutex_t m_mutex;
		pthread_mutexattr_t   m_mutex_attrs;
	};
}
#endif //_CPMUTEX_H
