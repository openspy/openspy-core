#ifndef _TASKSCHEDULER_H
#define _TASKSCHEDULER_H

#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/MessageQueue/rabbitmq/rmqConnection.h>
#include <OS/Net/NetServer.h>
#include <OS/Task.h>
#include <stack>
#include <string>
#include <map>

#include "ScheduledTask.h"

typedef int TaskSchedulerRequestType;
enum EThreadInitState {
	EThreadInitState_AllocThreadData,
	EThreadInitState_InitThreadData,
	EThreadInitState_DeallocThreadData
};


template<typename ReqClass, typename ThreadData>
class TaskScheduler {
	typedef bool (*TaskRequestHandler)(ReqClass, ThreadData *);
	typedef bool (*ListenerRequestHandler)(ThreadData *, std::map<std::string, std::string>);
	typedef ThreadData *(*ThreadDataFactory)(TaskScheduler<ReqClass, ThreadData> *, EThreadInitState, ThreadData *);
	typedef struct {
		TaskSchedulerRequestType type;
		TaskRequestHandler handler;
	} RequestHandlerEntry;
	typedef struct {
		std::string exchange;
		std::string routingKey;
		ListenerRequestHandler handler;
	} ListenerHandlerEntry;
	public:
		TaskScheduler(int num_tasks, INetServer *server) {
			//allocate tasks
			mp_server = server;
			m_tasks_setup = false;
			m_num_tasks = num_tasks;
			mp_thread_data_factory = DefaultThreadDataFactory;
			setupRootMQConnection();
			setupTasks();
		}
		~TaskScheduler() {
			//free tasks
		}
		bool AddRequestHandler(TaskSchedulerRequestType type, TaskRequestHandler handler) {
			RequestHandlerEntry entry;
			entry.type = type;
			entry.handler = handler;
			m_request_handlers.push_back(entry);
			return true;
		}
		bool AddRequestListener(std::string exchange, std::string routingKey, ListenerRequestHandler handler) {
			ListenerHandlerEntry entry;
			entry.exchange = exchange;
			entry.routingKey = routingKey;
			entry.handler = handler;
			m_listener_handlers.push_back(entry);
			return true;
		}
		void DeclareReady() {
			/*std::vector<ListenerHandlerEntry>::iterator itL;
			itL = m_listener_handlers.begin();
			while (itL != m_listener_handlers.end()) {
				ListenerHandlerEntry entry = *itL;
				mp_mqconnection->setReceiver(entry.exchange, entry.routingKey, MQListenerCallback);
				itL++;
			}
			mp_mqconnection->declareReady();*/

			//loop tasks, declaring them ready
			typename std::vector< ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *>::iterator it = m_tasks.begin();
			while(it != m_tasks.end()) {
				ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>> *task = *it;
				m_task_data[task] = mp_thread_data_factory(this, EThreadInitState_InitThreadData, m_task_data[task]);
				it++;
			}

		}
		void SetThreadDataFactory(ThreadDataFactory threadDataFactoryFunc) {
			mp_thread_data_factory = threadDataFactoryFunc;
		}

		void AddRequest(TaskSchedulerRequestType type, ReqClass request) {
			ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>>  *lowest = NULL, *task;
			typename std::vector< ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>>  *>::iterator it = m_tasks.begin();
			unsigned int lowest_count = UINT_MAX, size;
			while (it != m_tasks.end()) {
				task = *it;
				size = task->GetListSize();
				if (size < lowest_count) {
					lowest_count = size;
					lowest = task;
				}
				it++;
			}
			request.type = type;
			lowest->AddRequest(request);			
		}

		static ThreadData *DefaultThreadDataFactory(TaskScheduler<ReqClass, ThreadData> *scheduler, EThreadInitState state, ThreadData *data) {
			typename std::vector<ListenerHandlerEntry>::iterator it;
			typename std::vector<ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *>::iterator it2;
			struct timeval t;
			t.tv_usec = 0;
			t.tv_sec = 60;
			switch(state) {
				case EThreadInitState_AllocThreadData:					
					data = new ThreadData;
					data->mp_mqconnection = scheduler->mp_mqconnection->clone();
					data->mp_redis_connection = Redis::Connect(OS::g_redisAddress, t);
					data->scheduler = (void *)scheduler;
					data->server = scheduler->mp_server;
				break;
				case EThreadInitState_InitThreadData:
					it = scheduler->m_listener_handlers.begin();
					while(it != scheduler->m_listener_handlers.end()) {
						ListenerHandlerEntry entry = *it;
						data->mp_mqconnection->setReceiver(entry.exchange, entry.routingKey, MQListenerCallback, "", data);
						it++;
					}
					data->mp_mqconnection->declareReady();
					it2 = scheduler->m_tasks.begin();
					while (it2 != scheduler->m_tasks.end()) {
						ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >  *task = *it2;
						data->mp_thread = OS::CreateThread(ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>>::TaskThread, task, true);
						it2++;
					}
				break;
				case EThreadInitState_DeallocThreadData:
				break;
			}
			return data;
		}
		static void MQListenerCallback(std::string exchange, std::string routingKey, std::string message, void *extra) {
			ThreadData *thread_data = (ThreadData *)extra;
			TaskScheduler<ReqClass, ThreadData> *scheduler = (TaskScheduler<ReqClass, ThreadData> *)thread_data->scheduler;
			typename std::vector<ListenerHandlerEntry>::iterator it = scheduler->m_listener_handlers.begin();
			while (it != scheduler->m_listener_handlers.end()) {
				ListenerHandlerEntry entry = *it;
				if (entry.exchange.compare(exchange) == 0 && entry.routingKey.compare(routingKey) == 0) {
					entry.handler(thread_data, OS::KeyStringToMap(message));
				}
				it++;
			}
		}
		static void HandleRequestCallback(TaskScheduler<ReqClass, ThreadData> *scheduler,ReqClass request, ThreadData *data) {
			typename std::vector<RequestHandlerEntry>::iterator it = scheduler->m_request_handlers.begin();
			while (it != scheduler->m_request_handlers.end()) {
				RequestHandlerEntry entry = *it;
				if (entry.type == request.type) {
					entry.handler(request, data);
					break;
				}
				it++;
			}
		}
	protected:

		void setupTasks() {
			if(m_tasks_setup) return;
			m_tasks_setup = true;
			for(int i=0;i<m_num_tasks;i++) {
				ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *task = new ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >();
				task->SetScheduler(this);
				task->SetRequestCallback(HandleRequestCallback);
				m_tasks.push_back(task);
				m_task_data[task] = mp_thread_data_factory(this, EThreadInitState_AllocThreadData, NULL);
				task->SetThreadData(m_task_data[task]);
			}
		}

		void setupRootMQConnection() {
			std::string rabbitmq_address;
			std::string rabbitmq_user, rabbitmq_pass;
			std::string rabbitmq_vhost;

			OS::g_config->GetVariableString("", "rabbitmq_address", rabbitmq_address);

			OS::g_config->GetVariableString("", "rabbitmq_user", rabbitmq_user);
			OS::g_config->GetVariableString("", "rabbitmq_password", rabbitmq_pass);

			OS::g_config->GetVariableString("", "rabbitmq_vhost", rabbitmq_vhost);

			mp_mqconnection = new MQ::rmqConnection(OS::Address(rabbitmq_address), rabbitmq_user, rabbitmq_pass, rabbitmq_vhost);
		}

		ThreadDataFactory mp_thread_data_factory;
		std::vector<RequestHandlerEntry> m_request_handlers;
		std::vector<ListenerHandlerEntry> m_listener_handlers;
		MQ::IMQInterface* mp_mqconnection;

		std::vector<ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >  *> m_tasks;
		std::map<ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>> *, ThreadData *> m_task_data;

		INetServer *mp_server;

		int m_num_tasks;
		bool m_tasks_setup;
};

#endif //_TASKSCHEDULER_H