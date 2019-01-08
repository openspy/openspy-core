#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include <tasks/tasks.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>

namespace GP {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		INetPeer *findPeerByProfile(int profile_id, bool inc_ref = true);
		void InformStatusUpdate(int from_profileid, GPShared::GPStatus status);
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *GetAuthTask() { return mp_auth_tasks; };
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *GetUserTask() { return mp_user_tasks; };
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *GetProfileTask() { return mp_profile_tasks; };
		TaskScheduler<GPBackendRedisRequest, TaskThreadData> *GetGPTask() { return mp_gp_tasks; };
	private:
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
		TaskScheduler<GPBackendRedisRequest, TaskThreadData> *mp_gp_tasks;
	};
}
#endif //_GPSERVER_H