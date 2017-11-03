#include "Win32HiResTimer.h"
#include <string.h>
Win32HiResTimer::Win32HiResTimer() {
	memset(&m_start_time, 0, sizeof(m_start_time));
	memset(&m_end_time, 0, sizeof(m_end_time));

	QueryPerformanceFrequency(&m_frequency);
}
Win32HiResTimer::~Win32HiResTimer() {

}
void Win32HiResTimer::start() {
	QueryPerformanceCounter(&m_start_time);
}
void Win32HiResTimer::stop() {
	QueryPerformanceCounter(&m_end_time);
}
long long Win32HiResTimer::time_elapsed() {
	return m_end_time.QuadPart - m_start_time.QuadPart;
}

OS::HiResTimer *OS::HiResTimer::makeTimer() {
	return new Win32HiResTimer();
}