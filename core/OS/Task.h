#ifndef _OS_TASK_H
#define _OS_TASK_H
#include <queue>
namespace OS {
	#define TASK_SLEEP_TIME 200
	template<typename T>
	class Task {
	public:
		Task() {
			m_num_tasks = 0;
			mp_thread_poller = OS::CreateThreadPoller();
		}
		~Task() {
			delete mp_thread_poller;
		}
		void AddRequest(T data) {
			mp_mutex->lock();
			m_num_tasks++;
			m_request_list.push(data);
			mp_mutex->unlock();
			
			mp_thread_poller->signal();
		}
		int GetListSize() {
			return (int)m_num_tasks;
		}
	protected:
		CThreadPoller *mp_thread_poller;
		CThread	*mp_thread;
		CMutex *mp_mutex;
		std::queue<T> m_request_list;
		int m_num_tasks;
	};
}
#endif //_OS_TASK_H