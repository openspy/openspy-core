#ifndef _OS_REFCNT_H
#define _OS_REFCNT_H
#include <atomic>
namespace OS {
	class Ref {
		public:
			Ref() { m_ref_count.store(1); };
			void DecRef() { m_ref_count--; };
			void IncRef() { m_ref_count++; };
			int GetRefCount() { return m_ref_count.load(); };
		private:
			std::atomic<uint32_t> m_ref_count;
		
	};
};
#endif //_OS_REFCNT_H
