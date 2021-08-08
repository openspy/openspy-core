#include "PMutex.h"
#include <stdio.h>
namespace OS {
	CPMutex::CPMutex() {
		pthread_mutexattr_init(&m_mutex_attrs);
		pthread_mutexattr_settype(&m_mutex_attrs, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_mutex, &m_mutex_attrs);
	}
	CPMutex::~CPMutex() {
 		pthread_mutex_destroy(&m_mutex);
 		pthread_mutexattr_destroy(&m_mutex_attrs);
	}
	void CPMutex::lock() {
		pthread_mutex_lock(&m_mutex);
	}
	void CPMutex::unlock() {
		pthread_mutex_unlock(&m_mutex);
	}
}