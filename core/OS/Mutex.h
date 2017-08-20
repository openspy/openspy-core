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
				#else
					__sync_fetch_and_add(val, 1);
				#endif
			}
			static void SafeDecr(uint32_t *val) {
				#ifdef _WIN32
					InterlockedDecrement(val);
				#else
					__sync_fetch_and_sub(val, 1);
				#endif
			}
	};
}
#endif //_OS_MUTEX_H