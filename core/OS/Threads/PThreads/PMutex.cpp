#include "PMutex.h"
namespace OS {
	CPMutex::CPMutex() {
		pthread_mutex_init(&m_mutex, NULL);
	}
	CPMutex::~CPMutex() {
 		pthread_mutex_destroy(&m_mutex);
	}
	void CPMutex::lock() {
		pthread_mutex_lock(&m_mutex);
	}
	void CPMutex::unlock() {
		pthread_mutex_unlock(&m_mutex);
	}
}