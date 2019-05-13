#ifndef _PEERCHATSERVER_H
#define _PEERCHATSERVER_H
#include <stdint.h>
#include <tasks/tasks.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>

namespace Peerchat {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		INetPeer *findPeerByProfile(int profile_id, bool inc_ref = true);
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *GetAuthTask() { return mp_auth_tasks; };
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *GetUserTask() { return mp_user_tasks; };
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *GetProfileTask() { return mp_profile_tasks; };
		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *GetPeerchatTask() { return mp_peerchat_tasks; };
	private:
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *mp_peerchat_tasks;
	};
}
#endif //_GPSERVER_H