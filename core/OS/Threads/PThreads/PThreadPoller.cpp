#include "Win32ThreadPoller.h"
#include <stdio.h>
namespace OS {
	CPThreadPoller::CPThreadPoller() {
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_condition, NULL);
	}
	CPThreadPoller::~CPThreadPoller() {
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_condition);
	}
	bool CPThreadPoller::wait() {
		pthread_cond_wait(&m_condition, &m_mutex);
		return true;
	}
	void CPThreadPoller::signal() {
		pthread_cond_broadcast(&m_condition);
	}
}