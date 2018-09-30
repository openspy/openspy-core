#ifndef _CWIN32THREADPOLLER_H
#define _CWIN32THREADPOLLER_H
#include <windows.h>
#include <OS/ThreadPoller.h>
#include <OS/Mutex.h>
namespace OS {
	class CWin32ThreadPoller : public CThreadPoller {
	public:
		CWin32ThreadPoller();
		~CWin32ThreadPoller();
		bool wait(uint64_t time_ms = 0);
		void signal();
	private:
		HANDLE m_handle;
	};
}
#endif //_CWIN32MUTEX_H
