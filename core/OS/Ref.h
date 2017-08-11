#ifndef _OS_REFCNT_H
#define _OS_REFCNT_H
#include <OS/Mutex.h>
#include <assert.h>
namespace OS {
	class Ref {
		public:
			Ref() { m_ref_count=1; };
			void DecRef() { CMutex::SafeDecr(&m_ref_count); printf("Ref cnt now: %d - %p\n", m_ref_count, this); assert(m_ref_count >= 0); };
			void IncRef() { CMutex::SafeIncr(&m_ref_count); printf("Ref cnt now: %d - %p\n", m_ref_count, this); };
			int GetRefCount() { return m_ref_count; };
		private:
			uint32_t m_ref_count;
		
	};
};
#endif //_OS_REFCNT_H