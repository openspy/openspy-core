#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>

namespace SM {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *GetAuthTask() { return mp_auth_tasks; };
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *GetUserTask() { return mp_user_tasks; };
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *GetProfileTask() { return mp_profile_tasks; };
	private:
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
	};
}
#endif //_SMSERVER_H