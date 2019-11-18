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
		std::string getServerName() {  return "s"; };
		INetPeer *findPeerByProfile(int profile_id, bool inc_ref = true);
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *GetAuthTask() { return mp_auth_tasks; };
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *GetUserTask() { return mp_user_tasks; };
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *GetProfileTask() { return mp_profile_tasks; };
		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *GetPeerchatTask() { return mp_peerchat_tasks; };

		void OnUserMessage(std::string type, std::string from, std::string to, std::string message);
		void OnChannelMessage(std::string type, std::string from, ChannelSummary channel, std::string message, std::string target);

		//send 1 time message to anyone who is in a at least 1 channel with a user
		void OnChannelBroadcast(std::string type, std::string target, std::vector<int> channel_list, std::string message, bool includeSelf);
		void OnSetUserChannelKeys(ChannelSummary summary, UserSummary user_summary, OS::KVReader keys);
		void OnSetChannelKeys(ChannelSummary summary, OS::KVReader keys);

	private:
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *mp_peerchat_tasks;
	};
}
#endif //_GPSERVER_H