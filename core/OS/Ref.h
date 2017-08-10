#ifndef _OS_REFCNT_H
#define _OS_REFCNT_H
namespace OS {
	class Ref {
		public:
			Ref() { m_ref_count=1; };
			void DecRef() { m_ref_count--; };
			void IncRef() { m_ref_count++; };
			int GetRefCount() { return m_ref_count; };
		private:
			int m_ref_count;
		
	};
};
#endif //_OS_REFCNT_H