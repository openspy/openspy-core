#ifndef _TASKSCHEDULER_H
#define _TASKSCHEDULER_H

#define REDIS_PING_SECS 60

#include <OS/OpenSpy.h>
#include <OS/Config/AppConfig.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/MessageQueue/rabbitmq/rmqConnection.h>
#include <OS/Net/NetServer.h>
#include <OS/Cache/GameCache.h>
#include <OS/LinkedList.h>
#include <OS/Task.h>
#include <stack>
#include <string>
#include <map>
#include <hiredis/hiredis.h>

#include "ScheduledTask.h"

typedef int TaskSchedulerRequestType;
enum EThreadInitState {
	EThreadInitState_AllocThreadData,
	EThreadInitState_InitThreadData,
	EThreadInitState_DeallocThreadData
};


template<typename ReqClass, typename ThreadData>
class TaskScheduler {
	public:
		typedef bool (*TaskRequestHandler)(ReqClass, ThreadData *);
		typedef bool (*ListenerRequestHandler)(ThreadData *, std::string);
		typedef ThreadData *(*ThreadDataFactory)(TaskScheduler<ReqClass, ThreadData> *, EThreadInitState, ThreadData *);
		typedef struct {
			TaskSchedulerRequestType type;
			TaskRequestHandler handler;
		} RequestHandlerEntry;
		typedef struct {
			const char* exchange;
			const char *routingKey;
			ListenerRequestHandler handler;
		} ListenerHandlerEntry;
		typedef struct {
			ScheduledTask < ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >* lowest_task;
			unsigned int lowest_count;
		} AddRequestState;
	
		TaskScheduler(int num_tasks, INetServer *server, RequestHandlerEntry *requestHandlerTable, ListenerHandlerEntry *listenerHandlerTable) {
			//allocate tasks
			mp_server = server;
			m_tasks_setup = false;
			m_num_tasks = num_tasks;

			mp_mqconnection = NULL;

			mp_tasks = new OS::LinkedListHead<ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >*>();

			mp_request_handlers = requestHandlerTable;
			mp_listener_handlers = listenerHandlerTable;

			OS::DataCacheTimeout gameCacheTimeout;
			gameCacheTimeout.max_keys = 50;
			gameCacheTimeout.timeout_time_secs = 7200;
			mp_shared_game_cache = new OS::GameCache(m_num_tasks, gameCacheTimeout);

			mp_thread_data_factory = DefaultThreadDataFactory;

			if(listenerHandlerTable != NULL)
				setupRootMQConnection();
			setupTasks();
		}
		~TaskScheduler() {
			if(mp_mqconnection) {
				delete mp_mqconnection;
			}
			//free tasks
			OS::LinkedListIterator<ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >*, TaskScheduler<ReqClass, ThreadData>*> iterator(mp_tasks);
			iterator.Iterate(LLIterator_Task_Delete, this);

			delete mp_tasks;
			delete mp_shared_game_cache;

		}

		static bool LLIterator_Task_InitThreadData(ScheduledTask < ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> > *task, TaskScheduler<ReqClass, ThreadData> *scheduler) {
			scheduler->m_task_data[task] = scheduler->mp_thread_data_factory(scheduler, EThreadInitState_InitThreadData, scheduler->m_task_data[task]);
			return true;
		}
		void DeclareReady() {
			//loop tasks, declaring them ready
			OS::LinkedListIterator<ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >*, TaskScheduler<ReqClass, ThreadData>*> iterator(mp_tasks);
			iterator.Iterate(LLIterator_Task_InitThreadData, this);

		}
		void SetThreadDataFactory(ThreadDataFactory threadDataFactoryFunc) {
			mp_thread_data_factory = threadDataFactoryFunc;
		}
		static bool LLIterator_Task_Delete(ScheduledTask < ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >* task, TaskScheduler<ReqClass, ThreadData> *scheduler) {
			scheduler->mp_thread_data_factory(scheduler, EThreadInitState_DeallocThreadData, scheduler->m_task_data[task]);
			delete task;
			return true;
		}


		static bool LLIterator_Task_AddRequest(ScheduledTask < ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >* task, AddRequestState* state) {
			unsigned int size = task->GetListSize();
			if (size < state->lowest_count) {
				state->lowest_task = task;
				state->lowest_count = size;
			}
			return true;
		}
		void AddRequest(TaskSchedulerRequestType type, ReqClass request) {
			AddRequestState state;
			state.lowest_count = UINT_MAX;
			state.lowest_task = NULL;

			OS::LinkedListIterator<ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >*, AddRequestState*> iterator(mp_tasks);
			
			iterator.Iterate(LLIterator_Task_AddRequest, (AddRequestState *)&state);

			if (state.lowest_task != NULL) {
				request.type = type;
				state.lowest_task->AddRequest(request);
			}
		}

		static ThreadData *DefaultThreadDataFactory(TaskScheduler<ReqClass, ThreadData> *scheduler, EThreadInitState state, ThreadData *data) {
			int i = 0;
			void *redisReply = NULL;
			char address[64];
			uint16_t port = 6379;

			OS::LinkedListIterator<ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >*, ThreadData*> iterator(scheduler->mp_tasks);
			switch(state) {
				case EThreadInitState_AllocThreadData:					
					data = new ThreadData;
					if(scheduler->mp_mqconnection != NULL)
						data->mp_mqconnection = scheduler->mp_mqconnection->clone();
					

					OS::get_server_address_port(OS::g_redisAddress, address, port);
                    gettimeofday(&scheduler->m_last_redis_ping, NULL);

					data->redis_options = {0};
					REDIS_OPTIONS_SET_TCP(&data->redis_options, address, port);

					data->mp_redis_connection = redisConnectWithOptions(&data->redis_options);
					
					if(OS::g_redisUsername != NULL && OS::g_redisPassword != NULL) {
						redisReply = redisCommand(data->mp_redis_connection, "AUTH %s %s", OS::g_redisUsername, OS::g_redisPassword);
						freeReplyObject(redisReply);
					}
					
					data->scheduler = (void *)scheduler;
					data->server = scheduler->mp_server;
					data->mp_mutex = OS::CreateMutex();
				break;
				case EThreadInitState_InitThreadData:
					
					if (scheduler->mp_listener_handlers != NULL) {
						if (scheduler->mp_listener_handlers[i].handler != NULL) {
							do {
								data->mp_mqconnection->setReceiver(scheduler->mp_listener_handlers[i].exchange, scheduler->mp_listener_handlers[i].routingKey, MQListenerCallback, "", data);
							} while (scheduler->mp_listener_handlers[++i].handler != NULL);
						}
						data->mp_mqconnection->declareReady();
					}
					//iterator.Iterate(LLIterator_InitTaskThreadData, data); //this is already initalized at ScheduledTask ctor

				break;
				case EThreadInitState_DeallocThreadData:
					if(scheduler->mp_mqconnection != NULL)
						delete data->mp_mqconnection;
					//Redis::Disconnect(data->mp_redis_connection);
					//data->mp_redis_connection = NULL;
					delete data->mp_mutex;
					delete data;
				break;
			}
			return data;
		}
		static bool LLIterator_InitTaskThreadData(ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData> >* task, ThreadData* data) {
			data->mp_thread = OS::CreateThread(ScheduledTask<ReqClass, ThreadData*, TaskScheduler<ReqClass, ThreadData>>::TaskThread, task, true);
			return true;
		}
		static void MQListenerCallback(std::string exchange, std::string routingKey, std::string message, void *extra) {
			ThreadData *thread_data = (ThreadData *)extra;
			TaskScheduler<ReqClass, ThreadData> *scheduler = (TaskScheduler<ReqClass, ThreadData> *)thread_data->scheduler;
			ListenerRequestHandler handler = NULL;
			int i = 0;
			if (scheduler->mp_listener_handlers[i].handler != NULL) {
				do {
					if (strcmp(scheduler->mp_listener_handlers[i].exchange, exchange.c_str()) == 0 && strcmp(scheduler->mp_listener_handlers[i].routingKey, routingKey.c_str()) == 0) {
						handler = scheduler->mp_listener_handlers[i].handler;
						break;
					}
				} while (scheduler->mp_listener_handlers[++i].handler != NULL);
			}
			if(handler) {
				thread_data->mp_mutex->lock();
				handler(thread_data, message);
				thread_data->mp_mutex->unlock();
			}
		}
		static void HandleRequestCallback(TaskScheduler<ReqClass, ThreadData> *scheduler,ReqClass request, ThreadData *data) {
			TaskRequestHandler handler = NULL;
			int i = 0;
			if (scheduler->mp_request_handlers != NULL && scheduler->mp_request_handlers[i].handler != NULL) {
				do {
					if (scheduler->mp_request_handlers[i].type == request.type) {
						handler = scheduler->mp_request_handlers[i].handler;
						break;
					}
				} while (scheduler->mp_request_handlers[++i].handler != NULL);
			}
			if (handler) {
				data->mp_mutex->lock();
                scheduler->redisCheckConnection(data);
				handler(request, data);
				data->mp_mutex->unlock();
			}
		}
	protected:
        void redisCheckConnection(ThreadData *data) {
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            
            if(current_time.tv_sec - m_last_redis_ping.tv_sec > REDIS_PING_SECS) {
                void *redisReply = redisCommand(data->mp_redis_connection, "PING");
                if(redisReply) {
                    freeReplyObject(redisReply);
                    gettimeofday(&m_last_redis_ping, NULL);
                }

            }
            if(data->mp_redis_connection && data->mp_redis_connection->err) {
                int err = redisReconnect(data->mp_redis_connection);
                if(err == REDIS_OK) {
                    if(OS::g_redisUsername != NULL && OS::g_redisPassword != NULL) {
                        void *redisReply = redisCommand(data->mp_redis_connection, "AUTH %s %s", OS::g_redisUsername, OS::g_redisPassword);
                        freeReplyObject(redisReply);
                    }
                } else {
                    OS::LogText(OS::ELogLevel_Error, "Failed to reconnect to redis, err: %d\n", err);
                }
                
            }
        }
		void setupTasks() {
			if(m_tasks_setup) return;
			m_tasks_setup = true;
			for(int i=0;i<m_num_tasks;i++) {
				ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *task = new ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> >();
				task->SetScheduler(this);
				task->SetRequestCallback(HandleRequestCallback);
				mp_tasks->AddToList(task);
				m_task_data[task] = mp_thread_data_factory(this, EThreadInitState_AllocThreadData, NULL);
				m_task_data[task]->id = i;
				m_task_data[task]->shared_game_cache = mp_shared_game_cache;
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
		RequestHandlerEntry* mp_request_handlers;
		ListenerHandlerEntry* mp_listener_handlers;
		MQ::IMQInterface* mp_mqconnection;

		OS::LinkedListHead<ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData> > *>* mp_tasks;
		std::map<ScheduledTask<ReqClass, ThreadData *, TaskScheduler<ReqClass, ThreadData>> *, ThreadData *> m_task_data;

		INetServer *mp_server;

		int m_num_tasks;
		bool m_tasks_setup;

		OS::GameCache *mp_shared_game_cache;
    
        struct timeval m_last_redis_ping;

};

#endif //_TASKSCHEDULER_H
