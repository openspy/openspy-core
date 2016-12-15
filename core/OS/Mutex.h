#ifndef _OS_MUTEX_H
#define _OS_MUTEX_H
namespace OS {
	class CMutex {
		public:
			CMutex() { };
			~CMutex() {};
			virtual void lock() = 0;
			virtual void unlock() = 0;
	};
}
#endif //_OS_MUTEX_H