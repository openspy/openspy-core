#ifndef _SCHEDULEDTASK_H
#define _SCHEDULEDTASK_H
#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/Net/NetServer.h>
#include <OS/Task.h>
#include <string>
#include <map>

class TaskThreadData {
	public:
		MQ::IMQInterface *mp_mqconnection;
		Redis::Connection *mp_redis_connection;
		INetServer *server;
		OS::CThread *mp_thread;
		void *scheduler;
};

template<typename ReqClass, typename ThreadData>
class TaskScheduler;

template<typename ReqClass, typename ThreadData, typename Scheduler>
class ScheduledTask : public OS::Task<ReqClass> {
	typedef void (*HandleRequestCallback)(Scheduler *, ReqClass request, ThreadData data);
	public:
		ScheduledTask() : OS::Task<ReqClass>() {
			mp_task_scheduler = NULL;
			mp_thread_data = NULL;
			this->mp_mutex = OS::CreateMutex(); //move this into Task ctor...
			this->mp_thread = OS::CreateThread(ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >::TaskThread, this, true);
		}
		~ScheduledTask() {
			this->mp_thread->SignalExit(true);
			delete this->mp_thread;
			delete this->mp_mutex;
        }
		void SetThreadData(ThreadData data) {
			mp_thread_data = data;
		}
		void SetRequestCallback(HandleRequestCallback request_callback) {
			mp_request_callback = request_callback;
		}
		void SetScheduler(Scheduler *scheduler) {
			mp_task_scheduler = scheduler;
		}
		static void *TaskThread(OS::CThread *thread) {
			ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *task = (ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *)thread->getParams();
			while(thread->isRunning()) {
				task->StallForRequest();
			}
			return NULL; 
		}
		void StallForRequest() {
			this->mp_thread_poller->wait();
			this->mp_mutex->lock();
			bool empty = this->m_request_list.empty();
			ReqClass task_params;
			if(!empty) {
				task_params = this->m_request_list.front();
				this->m_request_list.pop();
				this->m_num_tasks--;
			}
			this->mp_mutex->unlock();
			if(!empty)
				mp_request_callback(mp_task_scheduler, task_params,mp_thread_data);

		}
	private:
	ThreadData mp_thread_data;
	HandleRequestCallback mp_request_callback;
	Scheduler *mp_task_scheduler;
};
#endif //_SCHEDULEDTASK_H