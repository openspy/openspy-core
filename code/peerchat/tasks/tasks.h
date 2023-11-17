#ifndef _PEERCHAT_TASKS_H
#define _PEERCHAT_TASKS_H
#include <string>
#include <algorithm>

#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <hiredis/hiredis.h>
#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/ProfileTasks.h>

#include <server/Peer.h>
#include <OS/tasks.h>

#define USER_EXPIRE_TIME 900

#define CHANNEL_EXPIRE_TIME 3600
#define CHANNEL_DELETE_EXPIRE_TIME 30

using namespace TaskShared;

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
		EChannelMode_UserCreated = 1 << 7, //+r - create isGlobal 0 owner by users ip
		EChannelMode_OpsObeyChannelLimit = 1 << 8, //+e OpsObeyChannelLimit
		EChannelMode_Auditorium = 1 << 9, //+u
		EChannelMode_Auditorium_ShowVOP = 1 << 10, //+q
		//
	};
	enum EUserModes {
		EUserMode_Quiet = 1 << 0, //+q
		EUserMode_Invisible = 1 << 1, // +i
		EUserMode_Gagged = 1 << 2, // +g
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
			EPeerchatRequestType_UpdateUserModes,
			EPeerchatRequestType_ListChannels,
			EPeerchatRequestType_SetChannelUserKeys,
			EPeerchatRequestType_GetChannelUserKeys,
			EPeerchatRequestType_SetUserKeys,
			EPeerchatRequestType_GetUserKeys,
			EPeerchatRequestType_SetChannelKeys,
			EPeerchatRequestType_GetChannelKeys,
			EPeerchatRequestType_UserKickChannel,
			EPeerchatRequestType_SetBroadcastToVisibleUsers,
			EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary,
			EPeerchatRequestType_SetBroadcastToVisibleUsers_SkipSource,
			EPeerchatRequestType_DeleteUser,
			EPeerchatRequestType_KeepaliveUser,
			EPeerchatRequestType_UserJoinEvents, //send invisible msg, do automatic modes etc
			EPeerchatRequestType_CreateUserMode,
			EPeerchatRequestType_ListUserModes,
			EPeerchatRequestType_ListUserModes_CacheLookup,
			EPeerchatRequestType_DeleteUserMode,
			EPeerchatRequestType_SetChanProps,
			EPeerchatRequestType_ListChanProps,
			EPeerchatRequestType_DeleteChanProps,
			EPeerchatRequestType_LookupGameInfo,
			EPeerchatRequestType_LookupGlobalUsermode,
			EPeerchatRequestType_RemoteKill_ByName,
			EPeerchatRequestType_UpdateChannelModes_BanMask,
			EPeerchatRequestType_OperCheck,
			EPeerchatRequestType_CountServerUsers,
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
	  EUserChannelFlag_Quiet = 1 << 8,
	  EUserChannelFlag_Banned = 1 << 9,
	  EUserChannelFlag_GameidPermitted = 1 << 10, //used for +p / chan client functionality
  };

	typedef struct ModeFlagMap {
		int flag;
		char character;
	} _ModeFlagMap;

	extern ModeFlagMap* channel_mode_flag_map;
	extern int num_channel_mode_flags;
	extern ModeFlagMap* user_mode_flag_map;
	extern int num_user_mode_flags;

	extern ModeFlagMap* user_join_chan_flag_map;
	extern int num_user_join_chan_flags;

	class ChannelUserSummary {
		public:
			int channel_id;
			int user_id;
			int modeflags;
			UserSummary userSummary;
	};
	class ChannelSummary {
		public:
			ChannelSummary() {
				channel_id = 0;
				basic_mode_flags = 0;
				limit = 0;
				memset(&created_at, 0, sizeof(created_at));
				memset(&topic_time, 0, sizeof(topic_time));
			}
			std::string channel_name;
			std::string entrymsg;
			int channel_id;
			int basic_mode_flags;
			std::string password;
			int limit;
			uv_timespec64_t created_at;

			std::string topic;
			uv_timespec64_t topic_time;
			std::string topic_user_summary;

			std::vector<ChannelUserSummary> users;
	};
	class UsermodeRecord {
		public:
			UsermodeRecord() {
				usermodeid = 0;
				isGlobal = false;
				profileid = 0;
				modeflags = 0;
				gameid = -1;
				has_gameid = false;
				memset(&expires_at, 0, sizeof(expires_at));
				memset(&set_at, 0, sizeof(set_at));
			}
			int usermodeid;
			std::string chanmask;
			std::string hostmask;
			std::string comment;
			std::string machineid;
			int profileid;
			int modeflags; //EUserChannelFlag
			bool isGlobal; //save to db, or only redis
			struct timeval expires_at; //or "expires in secs" when setting
			struct timeval set_at; //or "expires in secs" when setting
			UserSummary setByUserSummary;

			bool has_gameid; //false if null, true if exists
			int gameid; //-1 = unauthenticated
	};
	class ChanpropsRecord {
		public:
		ChanpropsRecord() {
			id = 0;
			onlyOwner = false;
			kickExisting = false;
			memset(&expires_at, 0, sizeof(expires_at));
			memset(&set_at, 0, sizeof(set_at));
		}
		int id;
		std::string channel_mask;
		std::string password;
		std::string entrymsg;
		std::string comment;
		struct timeval expires_at;
		std::string groupname;
		int limit;
		int modeflags;
		bool onlyOwner;
		std::string topic;
		struct timeval set_at;
		std::string setByNick;
		int setByPid;
		std::string setByHost;
		bool kickExisting;
	};
  class TaskResponse {
		public:
			TaskShared::WebErrorDetails error_details;
			OS::Profile profile;
			OS::User user;
			UserSummary summary;
			ChannelSummary channel_summary;
			ChannelUserSummary channelUserSummary;
			std::vector<ChannelSummary> channel_summaries;
			OS::KVReader kv_data;
			OS::KVReader kv_data_withnames; //used for wildcard GETCKEY, which must be output with names

			OS::GameData game_data;

			UsermodeRecord usermode;
			ChanpropsRecord chanprops;

			bool is_start;
			bool is_end;

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
			PeerchatBackendRequest() {
				peer = NULL;
			}
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
			std::string gamename;

			std::vector<int> channel_id_list;
			std::map<int, int> channel_flags;

			UsermodeRecord usermodeRecord;
			ChanpropsRecord chanpropsRecord;
	};
	

	bool Perform_SetUserDetails(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserJoinChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserJoinEvents(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserPartChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UserKickChannel(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_LookupChannelDetails(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_LookupUserDetailsByName(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_UpdateChannelModes(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UpdateUserModes(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SetChannelUserKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_GetChannelUserKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_SetUserKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_GetUserKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_SetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_SetBroadcastToVisibleUsers(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_DeleteUser(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_KeepaliveUser(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_ListUsermodes(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_ListUsermodes_Cached(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_DeleteUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_LookupGameInfo(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_LookupGlobalUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data);

	bool Perform_ListChanprops(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_SetChanprops(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_DelChanprops(PeerchatBackendRequest request, TaskThreadData* thread_data);

	bool Perform_OperCheck(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_CountServerUsers(PeerchatBackendRequest request, TaskThreadData* thread_data);

	bool Perform_RemoteKill_ByName(PeerchatBackendRequest request, TaskThreadData* thread_data);
	bool Perform_UpdateChannelModes_BanMask(PeerchatBackendRequest request, TaskThreadData* thread_data);
	
	bool Handle_PrivMsg(TaskThreadData *thread_data, std::string message);
	bool Handle_KeyUpdates(TaskThreadData *thread_data, std::string message);
	bool Handle_Broadcast(TaskThreadData *thread_data, std::string message);

  	void InitTasks();

	//user
	UserSummary LookupUserById(TaskThreadData *thread_data, int user_id);
	//

	//channel
	int GetPeerchatChannelID(TaskThreadData *thread_data);
	ChannelSummary LookupChannelById(TaskThreadData *thread_data, int channel_id);
	ChannelSummary CreateChannel(TaskThreadData *thread_data, std::string name);
	ChannelSummary GetChannelSummaryByName(TaskThreadData *thread_data, std::string name, bool create = false, UserSummary creator = UserSummary(), bool *created = NULL);
	void AddUserToChannel(TaskThreadData *thread_data, UserSummary user, ChannelSummary channel, int initial_flags);
	void RemoveUserFromChannel(TaskThreadData *thread_data, UserSummary user, ChannelSummary channel, std::string type, std::string remove_message, UserSummary target = UserSummary(), bool silent = false, int requiredChanUserModes = 0);
	std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id);
	int CountChannelUsers(TaskThreadData *thread_data, int channel_id);
	int CountUserChannels(TaskThreadData *thread_data, int user_id);
	void DeleteChannelById(TaskThreadData *thread_data, int channel_id);
	int LookupUserChannelModeFlags(TaskThreadData* thread_data, int channel_id, int user_id);
	void SendUpdateUserChanModeflags(TaskThreadData* thread_data, int channel_id, int user_id, int modeflags, int old_modeflags);
	void DeleteTemporaryUsermodesForChannel(TaskThreadData* thread_data, ChannelSummary channel);
	bool Is_Chankey_InjectKey(const char *key);
	int CountServerUsers(TaskThreadData* thread_data);
	void KeepaliveChannel(TaskThreadData* thread_data, ChannelSummary channel);

	void ApplyUserKeys(TaskThreadData* thread_data, std::string base_key, UserSummary userSummary, std::string user_base = "", bool show_private = false);

	UserSummary GetUserSummaryByName(TaskThreadData *thread_data, std::string name);
	extern const char *mp_pk_channel_name;

	//channel perms
	bool CheckActionPermissions(Peer *peer, std::string channel, int from_mode_flags, int to_mode_flags, int min_mode_flags);
	bool CheckChannelUserModeChange(TaskThreadData* thread_data, Peer *peer, std::string channel, int from_mode_flags, std::map<std::string, int> set_usermodes, std::map<std::string, int> unset_usermodes);
	bool TestChannelUserModeChangeItem(TaskThreadData* thread_data, Peer* peer, ChannelSummary channel_summary, std::string target_username, int from_mode_flags, int update_flags);
	EUserChannelFlag GetMinimumModeFlagsFromUpdateSet(int update_mode_flags);
	int GetUserChannelModeLevel(int modeflags);
	//

	int Create_StagingRoom_UsermodeRecords(ChannelSummary channel, PeerchatBackendRequest request, TaskThreadData* thread_data);

	int channelUserModesStringToFlags(std::string mode_string);
	std::string modeFlagsToModeString(int modeflags);

	UsermodeRecord GetUsermodeFromJson(json_t* item);

	int getEffectiveUsermode(TaskThreadData* thread_data, int channel_id, UserSummary summary, Peer* peer);
	json_t* GetJsonFromUserSummary(UserSummary summary);
	void WriteUsermodeToCache(UsermodeRecord usermode, TaskThreadData* thread_data);
	void AssociateUsermodeToChannel(UsermodeRecord record, ChannelSummary summary, TaskThreadData* thread_data);
	void LoadUsermodeFromCache(TaskThreadData* thread_data, std::string cacheKey, UsermodeRecord &record);
	json_t* UsermodeRecordToJson(UsermodeRecord record);

	ChanpropsRecord GetChanpropsFromJson(json_t* item);
	bool ApplyChanProps(ChannelSummary &summary);
	void SerializeChanpropsRecord(ChanpropsRecord record, std::ostringstream& ss);

	extern const char *peerchat_channel_exchange;
    extern const char *peerchat_client_message_routingkey;
	extern const char *peerchat_key_updates_routingkey;
	extern const char *peerchat_broadcast_routingkey;
	extern UserSummary		   *server_userSummary;

	void AddPeerchatTaskRequest(PeerchatBackendRequest request);
}
#endif //_MM_TASKS_H