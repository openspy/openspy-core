#include "UnixHiResTimer.h"
#include <string.h>
UnixHiResTimer::UnixHiResTimer() {
	memset(&m_start_time, 0, sizeof(m_start_time));
	memset(&m_end_time, 0, sizeof(m_end_time));
}
UnixHiResTimer::~UnixHiResTimer() {

}
void UnixHiResTimer::start() {
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &m_start_time);
}
void UnixHiResTimer::stop() {
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &m_end_time);
}
long long UnixHiResTimer::time_elapsed() {
	return m_end_time.tv_nsec - m_start_time.tv_nsec;
}

OS::HiResTimer *OS::HiResTimer::makeTimer() {
	return new UnixHiResTimer();
}