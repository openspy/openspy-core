#include "Win32ThreadPoller.h"
#include <stdio.h>
namespace OS {
	static int cwin32_thread_poller_cnt = 0;
	CWin32ThreadPoller::CWin32ThreadPoller() {
		char name[32];
		sprintf(name, "OS-Win32Poller-%d", cwin32_thread_poller_cnt++);
		m_handle = CreateEvent(
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			TEXT(name)  // object name
		);
	}
	CWin32ThreadPoller::~CWin32ThreadPoller() {
		CloseHandle(m_handle);
	}
	bool CWin32ThreadPoller::wait() {
		ResetEvent(m_handle);
		WaitForSingleObject(m_handle, INFINITE);
		return true;
	}
	void CWin32ThreadPoller::signal() {
		SetEvent(m_handle);
	}
}