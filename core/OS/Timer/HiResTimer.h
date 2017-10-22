#ifndef _HIRESTIMER_H
#define _HIRESTIMER_H
namespace OS {
	class HiResTimer {
	public:
		HiResTimer() { };
		~HiResTimer() { };
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual long long time_elapsed() = 0; //time in nanoseconds

		static HiResTimer *makeTimer();
	};
}
#endif // _HIRESTIMER_H