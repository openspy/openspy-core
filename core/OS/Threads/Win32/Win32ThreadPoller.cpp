#include "Win32ThreadPoller.h"
#include <stdio.h>
namespace OS {
	static int cwin32_thread_poller_cnt = 0;
	CWin32ThreadPoller::CWin32ThreadPoller() {
		char name[32];
		sprintf_s(name, sizeof(name), "OS-Win32Poller-%d", cwin32_thread_poller_cnt++);
		m_handle = CreateEvent(
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			TEXT(name)  // object name
		);
		m_signal_count = 0;
	}
	CWin32ThreadPoller::~CWin32ThreadPoller() {
		CloseHandle(m_handle);
	}
	bool CWin32ThreadPoller::wait(uint64_t time_ms) {
		ResetEvent(m_handle);
		if (m_signal_count == 0) {		
			if(time_ms == 0) {
				WaitForSingleObject(m_handle, INFINITE);
			} else {
				WaitForSingleObject(m_handle, time_ms);
			}
		}
		CMutex::SafeDecr(&m_signal_count);
		return true;
	}
	void CWin32ThreadPoller::signal() {
		CMutex::SafeIncr(&m_signal_count);
		SetEvent(m_handle);
	}
}