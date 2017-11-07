#include "Win32Mutex.h"
#include <stdio.h>
namespace OS {
	CWin32Mutex::CWin32Mutex() {
		InitializeCriticalSection(&m_critical_section);
	}
	CWin32Mutex::~CWin32Mutex() {
		DeleteCriticalSection(&m_critical_section);
	}
	void CWin32Mutex::lock() {
		EnterCriticalSection(&m_critical_section);
	}
	void CWin32Mutex::unlock() {
		LeaveCriticalSection(&m_critical_section);
	}
	
	CMutex *CreateMutex() {
		return new CWin32Mutex();
	}
}