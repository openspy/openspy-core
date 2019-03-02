#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Net/IOIfaces/SSLIOInterface.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#define FESL_SERVER_PORT 18300

namespace FESL {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		void OnUserAuth(std::string session_key, int userid, int profileid);

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