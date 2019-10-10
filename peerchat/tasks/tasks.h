#ifndef _PEERCHAT_TASKS_H
#define _PEERCHAT_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>


#include <OS/SharedTasks/tasks.h>
#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/ProfileTasks.h>

#include <server/Peer.h>

namespace Peerchat {
  enum EPeerchatRequestType {
			EPeerchatRequestType_SetUserDetails, //send user details (from /user cmd), get unique peerchat id
			EPeerchatRequestType_SendMessageToTarget, //send client/channel message
			EPeerchatRequestType_LookupUserDetails, //get user/realname/ip/gameid by nick
			EPeerchatRequestType_LookupChannelDetails,
			EPeerchatRequestType_UserJoinChannel,
	};
	class ChannelSummary {
		public:
		std::string channel_name;
		int channel_id;
		struct timeval created_at;
	};
	class ChannelUserSummary {
		public:
		int channel_id;
		int user_id;
		UserSummary userSummary;
	};
  class TaskResponse {
		public:
			TaskShared::WebErrorDetails error_details;
			OS::Profile profile;
			OS::User user;
			UserSummary summary;
			ChannelSummary channel_summary;
  };
	typedef void(*TaskCallback)(TaskResponse response_data, INetPeer *peer);

	class PeerchatBackendRequest {
		public:
			int type;
			Peer *peer;
			OS::User user;
			OS::Profile profile;
			UserSummary summary;
			ChannelSummary channel_summary;
			TaskCallback callback;

			std::string message_type;
			std::string message_target;
			std::string message;
	};
	

	bool Perform_ReserveNickname(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SetUserDetails(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Handle_Message(TaskThreadData *thread_data, std::string message);
	bool Handle_ChannelMessage(TaskThreadData *thread_data, std::string message);

  	TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server);

	//user
	UserSummary LookupUserById(TaskThreadData *thread_data, int user_id);
	//

	//channel
	int GetPeerchatChannelID(TaskThreadData *thread_data);
	ChannelSummary LookupChannelById(TaskThreadData *thread_data, int channel_id);
	ChannelSummary CreateChannel(TaskThreadData *thread_data, std::string name);
	ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name);
	void AddUserToChannel(TaskThreadData *thread_data, int user_id, int channel_id);
	std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id);
	extern const char *mp_pk_channel_name;
	//

	extern const char *peerchat_channel_exchange;
    extern const char *peerchat_client_message_routingkey;
	extern const char *peerchat_channel_message_routingkey;
}
#endif //_MM_TASKS_H