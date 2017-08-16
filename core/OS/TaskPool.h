#ifndef _OS_TASKPOOL_H
#define _OS_TASKPOOL_H
#include "Task.h"
namespace OS {
	template<typename T, typename R>
	class TaskPool {
	public:
		TaskPool(int num_threads) {
			for(int i=0;i<num_threads;i++) {
				T *task = new T();
				m_tasks.push_back(task);
			}
		}
		~TaskPool() {
			std::vector< T* >::iterator it = m_tasks.begin();
			while (it != m_tasks.end()) {
				delete *it;
				it++;
			}
		}
		void AddRequest(R data) {
			T *lowest = NULL, *task;
			std::vector< T* >::iterator it = m_tasks.begin();
			int lowest_count = 9999999999, size;
			while(it != m_tasks.end()) {
				task = *it;
				size = task->GetListSize();
				if(size < lowest_count) {
					lowest_count = size;
					lowest = task;
				}
				it++;
			}			
			task->AddRequest(data);
		}
		const std::vector< T* > getTasks() {
			return m_tasks;
		}
	protected:
		std::vector< T* > m_tasks;
	};
}
#endif //_OS_TASK_H