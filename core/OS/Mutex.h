#ifndef _OS_MUTEX_H
#define _OS_MUTEX_H
#include <stdint.h>
namespace OS {
	class CMutex {
		public:
			CMutex() { };
			virtual ~CMutex() {};
			virtual void lock() = 0;
			virtual void unlock() = 0;
						
			static void SafeIncr(uint32_t *val) {
				#ifdef _WIN32
					InterlockedIncrement(val);
				#endif
			}
			static void SafeDecr(uint32_t *val) {
				#ifdef _WIN32
					InterlockedDecrement(val);
				#endif
			}
	};
}
#endif //_OS_MUTEX_H