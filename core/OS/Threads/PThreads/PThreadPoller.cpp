#include <OS/Mutex.h>
#include "PThreadPoller.h"
#include <stdio.h>
#include <sys/time.h>
namespace OS {
	CPThreadPoller::CPThreadPoller() {
		m_signal_count = 0;
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_condition, NULL);
	}
	CPThreadPoller::~CPThreadPoller() {
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_condition);
	}
	bool CPThreadPoller::wait(uint64_t time_ms) {
		if(m_signal_count == 0) {
			if(time_ms == 0) {
				pthread_cond_wait(&m_condition, &m_mutex);
			} else {
				struct timeval tv;

				gettimeofday(&tv, NULL);

				struct timespec ts;
				ts.tv_sec = time(NULL) + time_ms / 1000;
				ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (time_ms % 1000);
				ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
				ts.tv_nsec %= (1000*1000*1000);
				
				pthread_cond_timedwait(&m_condition, &m_mutex, &ts);
			}
			
		}
		if(m_signal_count > 0)
			OS::CMutex::SafeDecr(&m_signal_count);
		if ((int)m_signal_count < 0) {
			m_signal_count = 0;
		}
		return true;
	}
	void CPThreadPoller::signal() {
		OS::CMutex::SafeIncr(&m_signal_count);
		if(m_signal_count > 0) {
			pthread_cond_broadcast(&m_condition);
		}
	}
}