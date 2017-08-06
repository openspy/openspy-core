#include "Win32Mutex.h"
#include <stdio.h>
namespace OS {
	CWin32Mutex::CWin32Mutex() {
		m_mutex = ::CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL); 
	}
	CWin32Mutex::~CWin32Mutex() {
 		CloseHandle(m_mutex);
	}
	void CWin32Mutex::lock() {
		WaitForSingleObject(&m_mutex, INFINITE);
	}
	void CWin32Mutex::unlock() {
		ReleaseMutex(&m_mutex);
	}
	
	CMutex *CreateMutex() {
		return new CWin32Mutex();
	}
}