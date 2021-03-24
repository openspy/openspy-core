#ifndef _CWIN32MUTEX_H
#define _CWIN32MUTEX_H
#include <windows.h>
#include <OS/Mutex.h>
namespace OS {
	class CWin32Mutex : public CMutex {
	public:
		CWin32Mutex();
		~CWin32Mutex();
		void lock();
		void unlock();
	private:
		CRITICAL_SECTION m_critical_section;

	};
}
#endif //_CWIN32MUTEX_H
