#ifndef _UNIXHIRESTIMER_H
#define _UNIXHIRESTIMER_H
#include <OS/Timer/HiResTimer.h>
#include <Windows.h>
#include <time.h>
class Win32HiResTimer : public OS::HiResTimer {
	public:
		Win32HiResTimer();
		~Win32HiResTimer();
		void start();
		void stop();
		long long time_elapsed();
	private:
		LARGE_INTEGER m_start_time, m_end_time, m_frequency;
};
#endif //_UNIXHIRESTIMER_H