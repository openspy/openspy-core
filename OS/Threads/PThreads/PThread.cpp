#include <OS/Thread.h>
#include "PThread.h"
#include <stdio.h>
namespace OS {
	void *CPThread::cpthread_thread(void *thread) {
		CPThread *t = (CPThread *)thread;
		t->m_entry(t);
		return NULL;
	}
	CPThread::CPThread(OS::ThreadEntry *entry, void *param, bool auto_start) : OS::CThread(entry, param, auto_start) {
		m_running = false;
		m_params = param;
		m_entry = entry;
		if(auto_start) {
			start();
		}
		

	}
	CPThread::~CPThread() {
	}
	void CPThread::start() {
		if(!m_running) {
			pthread_attr_t attr;
			pthread_create(&m_thread, NULL, cpthread_thread, (void *)this);
			m_running = true;
		}
	}
	void CPThread::stop() {
		if(m_running) {
			pthread_cancel(m_thread);
			m_running = false;
		}
	}
}