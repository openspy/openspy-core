#ifndef _OS_TASK_H
#define _OS_TASK_H
#include <queue>
#include <OS/Task.h>
namespace OS {
	#define TASK_SLEEP_TIME 200
	template<typename T>
	class Task {
	public:
		Task() {
			mp_thread_poller = OS::CreateThreadPoller();
		}
		~Task() {
			delete mp_thread_poller;
		}
		void AddRequest(T data) {
			mp_mutex->lock();
			m_request_list.push(data);
			mp_mutex->unlock();
			
			mp_thread_poller->signal();
		}
		int GetListSize() {
			int size = 0;
			mp_mutex->lock();
			size = m_request_list.size();
			mp_mutex->unlock();
			return size;
		}
	protected:
		CThreadPoller *mp_thread_poller;
		CThread	*mp_thread;
		CMutex *mp_mutex;
		std::queue<T> m_request_list;
	};
}
#endif //_OS_TASK_H