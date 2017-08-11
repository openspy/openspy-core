#ifndef _OS_TASK_H
#define _OS_TASK_H
#include <queue>
namespace OS {
	#define TASK_SLEEP_TIME 200
	template<typename T>
	class Task {
	public:
		Task() {

		}
		~Task() {

		}
		void AddRequest(T data) {
			mp_mutex->lock();
			m_request_list.push(data);
			mp_mutex->unlock();
		}
	protected:

		CThread	*mp_thread;
		CMutex *mp_mutex;
		std::queue<T> m_request_list;
	};
}
#endif //_OS_TASK_H