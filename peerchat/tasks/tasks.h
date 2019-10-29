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
	enum EOperPrivileges {
		OPERPRIVS_NONE = 0,
		OPERPRIVS_INVISIBLE = 1 << 0,
		OPERPRIVS_BANEXCEMPT = 1 << 1,
		OPERPRIVS_GETOPS = 1 << 2,
		OPERPRIVS_GLOBALOWNER = 1 << 3,
		OPERPRIVS_GETVOICE = 1 << 4,
		OPERPRIVS_OPEROVERRIDE = 1 << 5,
		OPERPRIVS_WALLOPS = 1 << 6,
		OPERPRIVS_KILL = 1 << 7,
		OPERPRIVS_FLOODEXCEMPT = 1 << 8,
		OPERPRIVS_LISTOPERS = 1 << 9,
		OPERPRIVS_CTCP = 1 << 10, //and ATM, etc
		OPERPRIVS_HIDDEN = 1 << 11,
		OPERPRIVS_SEEHIDDEN = 1 << 12,
		OPERPRIVS_MANIPULATE = 1 << 13, //can manipulate other peoples keys, etc
		OPERPRIVS_SERVMANAGE = 1 << 14,
		OPERPRIVS_WEBPANEL = 1 << 15, //permitted to log in to the web panel(doesn't serve any use on the actual server)
	};
	enum EChannelBasicModes {
		EChannelMode_NoOutsideMessages = 1 << 0, //+n
		EChannelMode_TopicProtect = 1 << 1, // +t
		EChannelMode_Moderated = 1 << 2, // +m
		EChannelMode_Private = 1 << 3, // +p
		EChannelMode_Secret = 1 << 4, // +s
		EChannelMode_InviteOnly = 1 << 5, // +i
		EChannelMode_StayOpen = 1 << 6, //+z??
		EChannelMode_Registered = 1 << 7, //+r 
		EChannelMode_OpsObeyChannelLimit = 1 << 8, //+e -- maybe "ops part of channel limit"?
		//
	};
  enum EPeerchatRequestType {
			EPeerchatRequestType_SetUserDetails, //send user details (from /user cmd), get unique peerchat id
			EPeerchatRequestType_SendMessageToTarget, //send client/channel message
			EPeerchatRequestType_LookupUserDetailsByName, //get user/realname/ip/gameid by nick
			EPeerchatRequestType_LookupChannelDetails,
			EPeerchatRequestType_UserJoinChannel,
			EPeerchatRequestType_UserPartChannel,
			EPeerchatRequestType_UpdateChannelModes,
			EPeerchatRequestType_ListChannels,
			EPeerchatRequestType_SetChannelKeys,
			EPeerchatRequestType_GetChannelKeys,
	};

  enum EUserChannelFlag {
	  EUserChannelFlag_IsInChannel = 1 << 0,
	  EUserChannelFlag_Voice = 1 << 1,
	  EUserChannelFlag_HalfOp = 1 << 2,
	  EUserChannelFlag_Op = 1 << 3,
	  EUserChannelFlag_Owner = 1 << 4,
	  EUserChannelFlag_Invisible = 1 << 5,
	  EUserChannelFlag_Gagged = 1 << 6,
	  EUserChannelFlag_Invited = 1 << 7,
  };

	typedef struct ModeFlagMap {
		int flag;
		char character;
	} _ModeFlagMap;
	extern int num_mode_flags;
	extern ModeFlagMap *mode_flag_map;
	class ChannelUserSummary {
		public:
		int channel_id;
		int user_id;
		int modeflags;
		UserSummary userSummary;
	};
	class ChannelSummary {
		public:
		std::string channel_name;
		int channel_id;
		int basic_mode_flags;
		std::string password;
		int limit;
		struct timeval created_at;

		std::string topic;
		struct timeval topic_time;
		std::string topic_user_summary;

		std::vector<ChannelUserSummary> users;
	};
  class TaskResponse {
		public:
			TaskShared::WebErrorDetails error_details;
			OS::Profile profile;
			OS::User user;
			UserSummary summary;
			ChannelSummary channel_summary;
			std::vector<ChannelSummary> channel_summaries;
			OS::KVReader kv_data;
  };
  typedef void(*TaskCallback)(TaskResponse response_data, Peer *peer);

  class ChannelModifyRequest {
	public:
		ChannelModifyRequest() {
			set_mode_flags = 0;		
			unset_mode_flags = 0;
			update_limit = false;
			update_password = false;
			update_topic = false;
		}
		int set_mode_flags;
		int unset_mode_flags;
		bool update_password;
		bool update_limit;
		int limit;
		std::string password;

		bool update_topic;
		std::string topic;

		std::map<std::string, int> set_usermodes;
		std::map<std::string, int> unset_usermodes;

		OS::KVReader kv_data;
  };

	class PeerchatBackendRequest {
		public:
			int type;
			Peer *peer;
			OS::User user;
			OS::Profile profile;
			UserSummary summary;
			ChannelSummary channel_summary;
			TaskCallback callback;

			ChannelModifyRequest channel_modify;

			std::string message_type;
			std::string message_target;
			std::string message;
	};
	

	bool Perform_SetUserDetails(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserPartChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_LookupChannelDetails(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_LookupUserDetailsByName(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_UpdateChannelModes(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	
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
	ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name, bool create);
	void AddUserToChannel(TaskThreadData *thread_data, int user_id, int channel_id, int initial_flags);
	void RemoveUserFromChannel(TaskThreadData *thread_data, int user_id, int channel_id, std::string type);
	std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id);

	UserSummary GetUserSummaryByName(TaskThreadData *thread_data, std::string name);
	extern const char *mp_pk_channel_name;
	//

	extern const char *peerchat_channel_exchange;
    extern const char *peerchat_client_message_routingkey;
	extern const char *peerchat_channel_message_routingkey;
}
#endif //_MM_TASKS_H