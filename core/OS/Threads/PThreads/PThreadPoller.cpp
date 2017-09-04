#include "PThreadPoller.h"
#include <stdio.h>
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
	bool CPThreadPoller::wait() {
		if(m_signal_count == 0) {
			pthread_cond_wait(&m_condition, &m_mutex);
		}
		m_signal_count = 0;
		return true;
	}
	void CPThreadPoller::signal() {
		if(m_signal_count == 0) {
			m_signal_count++;
			pthread_cond_broadcast(&m_condition);
		}
	}
}