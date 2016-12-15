#ifndef _OS_TASK_H
#define _OS_TASK_H
#include <vector>
namespace OS {
	template<typename T>
	class Task {
	public:
		Task() {

		}
		~Task() {

		}
		void AddRequest(T data) {
			mp_mutex->lock();
			m_request_list.push_back(data);
			mp_mutex->unlock();
		}
		virtual void Process() = 0;
	protected:

		CThread	*mp_thread;
		CMutex *mp_mutex;
		std::vector<T> m_request_list;
	};
}
#endif //_OS_TASK_H