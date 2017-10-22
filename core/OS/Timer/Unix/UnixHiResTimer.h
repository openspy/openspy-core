#ifndef _UNIXHIRESTIMER_H
#define _UNIXHIRESTIMER_H
#include <OS/Timer/HiResTimer.h>
#include <time.h>
class UnixHiResTimer : public OS::HiResTimer {
	public:
		UnixHiResTimer();
		~UnixHiResTimer();
		void start();
		void stop();
		long long time_elapsed();
	private:
		struct timespec m_start_time, m_end_time;
};
#endif //_UNIXHIRESTIMER_H