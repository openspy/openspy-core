#ifndef _PEERCHATSERVER_H
#define _PEERCHATSERVER_H
#include <stdint.h>
#include <tasks/tasks.h>
#include <OS/Net/NetServer.h>

int match(const char* mask, const char* name);
int match2(const char* mask, const char* name);
namespace Peerchat {
	class Server : public INetServer {
	public:
		Server();
		~Server();
		void tick();
		std::string getServerName() {  return "s"; };
		INetPeer *findPeerByProfile(int profile_id, bool inc_ref = true);

		void OnUserMessage(std::string type, UserSummary from, UserSummary to, std::string message);
		void OnChannelMessage(std::string type, ChannelUserSummary from, ChannelSummary channel, std::string message, ChannelUserSummary target, bool includeSelf, int requiredChanFlags, int requiredOperFlags, int onlyVisibleTo);

		//send 1 time message to anyone who is in a at least 1 channel with a user
		void OnChannelBroadcast(std::string type, UserSummary target, std::map<int, int> channel_list, std::string message, bool includeSelf);
		void OnSetUserChannelKeys(ChannelSummary summary, UserSummary user_summary, OS::KVReader keys);
		void OnSetChannelKeys(ChannelSummary summary, OS::KVReader keys);

		void OnUpdateChanUsermode(int channel_id, UserSummary user_summary, int new_modeflags);

		void OnKillUser(UserSummary user_summary, std::string reason);

	private:
	};
}
#endif //_GPSERVER_H