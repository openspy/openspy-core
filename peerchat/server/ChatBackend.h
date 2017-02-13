#ifndef _CHAT_BACKEND_H
#define _CHAT_BACKEND_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <vector>
#include <map>
#include <string>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/adapters/libevent.h>
#undef _WINSOCK2API_

#define CHAT_BACKEND_TICK 200

namespace Chat {
	class Driver;
	class Peer;
	extern redisContext *mp_redis_connection;

	struct _ChatQueryRequest;
	enum EChatBackendResponseError {
		EChatBackendResponseError_NoError,
		EChatBackendResponseError_NoUser_OrChan,
	};
	typedef struct _ChatClientInfo {
		int client_id; //redis id
		std::string name;
		std::string user;
		std::string realname;
		std::string hostname;
		OS::Address ip;
	} ChatClientInfo;

	enum EChanClientFlags {
		EChanClientFlags_None = 0,
		EChanClientFlags_Voice = 1,
		EChanClientFlags_HalfOp = 2,
		EChanClientFlags_Op = 4,
		EChanClientFlags_Owner = 8,
		EChanClientFlags_Gagged = 16,
		EChanClientFlags_Banned = 32,
		EChanClientFlags_Invisible = 64,
	};
	typedef struct _ChatChanClientInfo {
		uint32_t m_client_flags;
	} ChatChanClientInfo;

	enum EChannelModeFlags {
		EChannelModeFlags_Private = 1,
		EChannelModeFlags_Secret = 2,
		EChannelModeFlags_TopicOpOnly = 4,
		EChannelModeFlags_NoOutsideMsgs = 8,
		EChannelModeFlags_Moderated = 16,
		EChannelModeFlags_Auditormium = 32,
		EChannelModeFlags_VOPAuditormium = 64,
		EChanClientFlags_Invisible = 128,
		EChanClientFlags_InviteOnly = 256,
		EChanClientFlags_StayOpened = 512,
		EChanClientFlags_OnlyOwner = 1024, //only owner can change modes
	};

	typedef struct _ChatChannelInfo {
		int channel_id; //redis id
		std::string name;
		std::string topic;
		std::string topic_setby;
		int topic_seton;
		std::vector<int> clients; //client ids

		int modeflags;
		std::string password;
		int limit;

		std::string entry_msg;
		std::map<std::string, std::string> custom_keys; //setkey/getkey, also holds groupname(/setgroup)
	} ChatChannelInfo;

	typedef struct _ChatQueryResponse {
		ChatChannelInfo channel_info;
		ChatClientInfo client_info;
		EChatBackendResponseError error;
	} ChatQueryResponse;
	typedef void (*ChatQueryCB)(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
	enum EChatQueryRequestType {
		EChatQueryRequestType_GetClientByName,
		EChatQueryRequestType_UpdateOrInsertClient,
		EChatQueryRequestType_SendClientMessage,
		EChatQueryRequestType_SendChannelMessage,
		EChatQueryRequestType_Find_OrCreate_Channel,
		EChatQueryRequestType_Find_Channel,
		EChatQueryRequestType_AddUserToChannel,
		EChatQueryRequestType_RemoveUserFromChannel,
	};
	typedef struct _ChatQueryRequest {
		EChatQueryRequestType type;
		ChatQueryCB callback;
		Peer *peer;
		void *extra;
		std::string query_name; //client/chan name
		std::string message; //for client messages

		ChatQueryResponse query_data;
	} ChatQueryRequest;

	class ChatBackendTask : public OS::Task<ChatQueryRequest> {
		public:
			ChatBackendTask();
			~ChatBackendTask();
			static ChatBackendTask *getQueryTask();
			static void Shutdown();

			void AddDriver(Chat::Driver *driver);
			void RemoveDriver(Chat::Driver *driver);

			static void SubmitGetClientInfoByName(std::string name, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitClientInfo(ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitClientMessage(int target_id, std::string message, ChatQueryCB cb, Peer *peer, void *extra);
			static void SubmitFind_OrCreateChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel);
			static void SubmitFindChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel);

			static void SubmitAddUserToChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel);
			static void SubmitRemoveUserFromChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel);
			void flagPushTask();
		private:
			static void *TaskThread(OS::CThread *thread);

			static void *setup_redis_async(OS::CThread *thread);

			static void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata);

			void PerformGetClientInfoByName(ChatQueryRequest task_params);
			void PerformUpdateOrInsertClient(ChatQueryRequest task_params);
			void PerformSendClientMessage(ChatQueryRequest task_params);
			void PerformFind_OrCreateChannel(ChatQueryRequest task_params, bool no_create = false);
			void PerformSendAddUserToChannel(ChatQueryRequest task_params);
			void PerformSendRemoveUserFromChannel(ChatQueryRequest task_params);


			ChatChannelInfo GetChannelByName(std::string name);
			ChatChannelInfo GetChannelByID(int id);
			ChatChannelInfo CreateChannel(std::string name);


			void LoadClientInfoByID(ChatClientInfo &info, int client_id);
			static std::string ClientInfoToKVString(ChatClientInfo info);
			static ChatClientInfo ClientInfoFromKVString(const char *str);
			void SendClientMessageToDrivers(int target_id, ChatClientInfo user, const char *msg);
			void SendClientJoinChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel);
			void SendClientPartChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel);

			static std::string ChannelInfoToKVString(ChatChannelInfo info);
			static ChatChannelInfo ChannelInfoFromKVString(const char *str);

			std::vector<Chat::Driver *> m_drivers;
			redisContext *mp_redis_connection;
			redisContext *mp_redis_async_retrival_connection;
			redisAsyncContext *mp_redis_async_connection;
			struct event_base *mp_event_base;

			bool m_flag_push_task;

			OS::CThread *mp_async_thread;

			static ChatBackendTask *m_task_singleton;
	};
};

#endif //_MM_QUERY_H