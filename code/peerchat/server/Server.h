#ifndef _PEERCHATSERVER_H
#define _PEERCHATSERVER_H
#include <stdint.h>
#include <tasks/tasks.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>

int match(const char* mask, const char* name);
int match2(const char* mask, const char* name);
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

		void OnUserMessage(std::string type, UserSummary from, UserSummary to, std::string message);
		void OnChannelMessage(std::string type, ChannelUserSummary from, ChannelSummary channel, std::string message, ChannelUserSummary target, bool includeSelf, int requiredChanFlags, int requiredOperFlags, int onlyVisibleTo);

		//send 1 time message to anyone who is in a at least 1 channel with a user
		void OnChannelBroadcast(std::string type, UserSummary target, std::map<int, int> channel_list, std::string message, bool includeSelf);
		void OnSetUserChannelKeys(ChannelSummary summary, UserSummary user_summary, OS::KVReader keys);
		void OnSetChannelKeys(ChannelSummary summary, OS::KVReader keys);

		void OnUpdateChanUsermode(int channel_id, UserSummary user_summary, int new_modeflags);

	private:
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *mp_peerchat_tasks;
	};
}
#endif //_GPSERVER_H