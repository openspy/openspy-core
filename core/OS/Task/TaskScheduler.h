#ifndef _TASKSCHEDULER_H
#define _TASKSCHEDULER_H

#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/Net/NetServer.h>
#include <OS/Task.h>
#include <string>
#include <map>

#include "DefaultTask.h"

typedef int TaskSchedulerRequestType;
enum EThreadInitState {
	EThreadInitState_AllocThreadData,
	EThreadInitState_InitThreadData,
	EThreadInitState_DeallocThreadData
};


template<typename ReqClass, typename ThreadData>
class TaskScheduler {
	typedef bool (*TaskRequestHandler)(TaskScheduler<ReqClass, ThreadData> *, TaskSchedulerRequestType, ReqClass, ThreadData  *);
	typedef bool (*ListenerRequestHandler)(TaskScheduler<ReqClass, ThreadData> *, ThreadData  *, std::map<std::string, std::string>);
	typedef struct {
		TaskSchedulerRequestType type;
		TaskRequestHandler handler;
	} RequestHandlerEntry;
	typedef struct {
		std::string exchange;
		std::string routingKey;
		std::string message_type;
		ListenerRequestHandler handler;
	} ListenerHandlerEntry;
	public:
		TaskScheduler(int num_tasks) {
			//allocate tasks
		}
		~TaskScheduler() {
			//free tasks
		}
		bool AddRequestHandler(TaskSchedulerRequestType type, TaskRequestHandler handler) {
			RequestHandlerEntry entry;
			entry.type = type;
			entry.handler = handler;
			m_request_handlers.push_back(entry);
		}
		bool AddRequestListener(std::string exchange, std::string routingKey,std::string message_type, ListenerRequestHandler handler) {
			ListenerHandlerEntry entry;
			entry.exchange = exchange;
			entry.routingKey = routingKey;
			entry.message_type = message_type;
			entry.handler = handler;
			m_listener_handlers.push_back(entry);
		}
		void DeclareReady() {
			//loop tasks, declaring them ready
		}
		void SetThreadDataFactory(ThreadData *(*threadDataFactoryFunc)(TaskScheduler<ReqClass, ThreadData> *, EThreadInitState, ThreadData *)) {
			mp_thread_data_factory = threadDataFactoryFunc;
		}

		void AddRequest(TaskSchedulerRequestType type, ReqClass request) {
			//loop tasks, finding least busy, and pushing this request to its task queue
		}

		static TaskThreadData *DefaultThreadDataFactory(TaskScheduler<ReqClass, TaskThreadData> *scheduler, EThreadInitState state, TaskThreadData *data) {
			return NULL;
		}
	protected:

		ThreadData *(*threadDataFactoryFunc)(TaskScheduler<ReqClass, ThreadData> *, EThreadInitState, ThreadData *) mp_thread_data_factory;
		std::vector<RequestHandlerEntry> m_request_handlers;
		std::vector<ListenerHandlerEntry> m_listener_handlers;

		std::vector<DefaultTask *> m_tasks;
};

#endif //_TASKSCHEDULER_H