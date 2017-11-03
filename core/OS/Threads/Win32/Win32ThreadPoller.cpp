#include "Win32ThreadPoller.h"
#include <stdio.h>
namespace OS {
	CWin32ThreadPoller::CWin32ThreadPoller() {
		m_handle = CreateEvent(
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			TEXT("OS-Win32Poller")  // object name
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