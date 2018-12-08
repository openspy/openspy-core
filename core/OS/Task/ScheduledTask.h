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
	typedef void (*HandleRequestCallback)(Scheduler *, ReqClass request, TaskThreadData *data);
	public:
		ScheduledTask() {
			mp_task_scheduler = NULL;
			mp_thread_data = NULL;
			mp_thread = OS::CreateThread(ScheduledTask<ReqClass, TaskThreadData *, TaskScheduler<ReqClass, ThreadData> >::TaskThread, this, true);
		}
		~ScheduledTask() {
			mp_thread->SignalExit(true);
			delete mp_thread;
        }
		void SetThreadData(TaskThreadData *data) {
			mp_thread_data = data;
		}
		void SetRequestCallback(HandleRequestCallback request_callback) {
			mp_request_callback = request_callback;
		}
		void SetScheduler(Scheduler *scheduler) {
			mp_task_scheduler = scheduler;
		}
		static void *TaskThread(OS::CThread *thread) {
			ScheduledTask<ReqClass, TaskThreadData *, TaskScheduler<ReqClass, ThreadData> > *task = (ScheduledTask<ReqClass, TaskThreadData *, TaskScheduler<ReqClass, ThreadData> > *)thread->getParams();
			while(thread->isRunning()) {
				task->StallForRequest();
			}
			return NULL; 
		}
		void StallForRequest() {
			mp_thread_poller->wait();
			mp_mutex->lock();
			ReqClass task_params = m_request_list.front();
			m_request_list.pop();
			m_num_tasks--;
			mp_mutex->unlock();
			mp_request_callback(mp_task_scheduler, task_params,mp_thread_data);

		}
	private:
	TaskThreadData *mp_thread_data;
	HandleRequestCallback mp_request_callback;
	Scheduler *mp_task_scheduler;
	int m_num_tasks;
};
#endif //_SCHEDULEDTASK_H