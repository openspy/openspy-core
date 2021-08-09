#ifndef _SCHEDULEDTASK_H
#define _SCHEDULEDTASK_H
#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/Net/NetServer.h>
#include <OS/Task.h>
#include <string>
#include <map>

#define SCHEDULED_TASK_WAIT_TIME 500

class TaskThreadData {
	public:
		MQ::IMQInterface *mp_mqconnection;
		Redis::Connection *mp_redis_connection;
		INetServer *server;
		OS::CThread *mp_thread;
		void *scheduler;
		int id; //task thread index
		OS::GameCache *shared_game_cache;
		OS::CMutex *mp_mutex;
};

template<typename ReqClass, typename ThreadData>
class TaskScheduler;

template<typename ReqClass, typename ThreadData, typename Scheduler>
class ScheduledTask : public OS::Task<ReqClass>, public OS::LinkedList<ScheduledTask<ReqClass, ThreadData, Scheduler> *> {
	typedef void (*HandleRequestCallback)(Scheduler *, ReqClass request, ThreadData data);
	public:
		ScheduledTask() : OS::Task<ReqClass>(), OS::LinkedList<ScheduledTask<ReqClass, ThreadData, Scheduler>*>() {
			mp_task_scheduler = NULL;
			mp_thread_data = NULL;
			this->mp_mutex = OS::CreateMutex(); //move this into Task ctor...
			this->mp_thread = OS::CreateThread(ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >::TaskThread, this, true);
		}
		~ScheduledTask() {
			this->mp_thread->SignalExit(true, this->mp_thread_poller);
			delete this->mp_thread;
			delete this->mp_mutex;
			this->mp_thread = NULL;
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
				task->StallForRequest(thread);
			}
			return NULL; 
		}
		void StallForRequest(OS::CThread *thread) {			
			if(this->m_request_list.empty())
				this->mp_thread_poller->wait(SCHEDULED_TASK_WAIT_TIME);
			if(!thread->isRunning()) return;
			this->mp_mutex->lock();
			bool empty = this->m_request_list.empty();
			ReqClass task_params;
			if(!empty) {
				task_params = this->m_request_list.front();
				this->m_request_list.pop();
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