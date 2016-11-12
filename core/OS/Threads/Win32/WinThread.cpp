#include <Core/CThread.h>
#include "WinThread.h"
namespace OS {
	DWORD CWin32Thread::cwin32thread_thread(void *thread) {
		CPThread *t = (CWin32Thread *)thread;
		t->m_entry(t);
	}
	CWin32Thread::CWin32Thread(OS::ThreadEntry *entry, void *param, bool auto_start) : OS::CThread(entry, param, auto_start) {
		m_running = false;
		m_params = param;
		if(auto_start) {
			start();
		}
		m_entry = entry;

	}
	CWin32Thread::~CWin32Thread() {
	}
	void CWin32Thread::start() {
		if(!m_running) {
			m_handle = CreateThread(NULL, 0,cwin32thread_thread, 0, &m_threadid);
			m_running = true;
		}
	}
	void CWin32Thread::stop() {
		if(m_running) {
			TerminateThread(m_handle, 0);
			m_running = false;
		}
	}
}
