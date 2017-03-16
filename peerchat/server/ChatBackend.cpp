#include "ChatBackend.h"
#include "ChatPeer.h"
#include <sstream>
#include <OS/KVReader.h>
#include <OS/legacy/helpers.h>
namespace Chat {

	/*
		Table for basic channel permissions
			Missing: 
				Kick/Invisible
				Usermode apply - handle inside IRCPeer??
				Chanprops apply - cache applied masks and mass-apply to matching
				Channel create chanprops defaulting
				client info applied usermode retrieving
				KILL/KLINE

			std::vector<ChatStoredUserMode> GetClientUsermodes(ChatClientInfo info);
			ChatStoredChanProps GetChannelChanProps(std::string channel_name);
			void ApplyChannelProps(ChatChannelInfo &channel, bool send_mq = false);
	*/
	struct _ModeFlagPermissions {
		int flag;
		const char *friendly_name;
		EChanClientFlags required_flag;
	} ModeFlagPermissions[] = {
		{EChannelModeFlags_Private, "Private Mode", EChanClientFlags_HalfOp},
		{EChannelModeFlags_Secret, "Secret Mode", EChanClientFlags_HalfOp},
		{EChannelModeFlags_TopicOpOnly, "Topic Lock Mode", EChanClientFlags_HalfOp},
		{EChannelModeFlags_NoOutsideMsgs, "No Outside Messages Mode", EChanClientFlags_HalfOp},
		{EChannelModeFlags_Moderated, "Moderated Mode", EChanClientFlags_HalfOp},
		{EChannelModeFlags_Auditormium, "Auditorium Mode", EChanClientFlags_Op},
		{EChannelModeFlags_VOPAuditormium, "VOPAuditorium Mode", EChanClientFlags_Op},
		{EChannelModeFlags_InviteOnly, "Invite Only Mode", EChanClientFlags_Op},
		{EChannelModeFlags_StayOpened, "Stay Open Mode", EChanClientFlags_Owner},
		{EChannelModeFlags_OnlyOwner, "Only Owner Mode", EChanClientFlags_Owner},
	};
	
	const char *mp_pk_name = "CLIENTID";
	const char *mp_chan_pk_name = "CHANID";
	const char *mp_usermode_pk_name = "USERMODEID";
	const char *mp_chanprops_pk_name = "CHANPROPSID";
	const char *chat_messaging_channel = "chat.messaging";
	ChatBackendTask *ChatBackendTask::m_task_singleton = NULL;
	ChatBackendTask::ChatBackendTask() {
		m_flag_push_task = false;
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);
		mp_redis_async_retrival_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		mp_async_thread = OS::CreateThread(setup_redis_async, this, true);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(ChatBackendTask::TaskThread, this, true);

		m_permission_name_map[EChanClientFlags_Owner] = "Channel Owner";
		m_permission_name_map[EChanClientFlags_Op] = "Channel Operator";
		m_permission_name_map[EChanClientFlags_HalfOp] = "Channel Half-Operator";
		m_permission_name_map[EChanClientFlags_Voice] = "Channel Voice";
	}
	ChatBackendTask::~ChatBackendTask() {
		delete mp_thread;
		delete mp_async_thread;
		delete mp_mutex;

		redisFree(mp_redis_connection);
		redisFree(mp_redis_async_retrival_connection);
		redisAsyncFree(mp_redis_async_connection);
		event_base_free(mp_event_base);
	}
	void ChatBackendTask::flagPushTask() {
		m_flag_push_task = true;
	}
	ChatBackendTask *ChatBackendTask::getQueryTask() {
		if(!ChatBackendTask::m_task_singleton) {
			ChatBackendTask::m_task_singleton = new ChatBackendTask();
		}
		return ChatBackendTask::m_task_singleton;
	}
	void ChatBackendTask::Shutdown() {
		ChatBackendTask *task = getQueryTask();
		
		delete task;
	}

	void ChatBackendTask::AddDriver(Chat::Driver *driver) {
		m_drivers.push_back(driver);
	}
	void ChatBackendTask::RemoveDriver(Chat::Driver *driver) {

	}

	void ChatBackendTask::SubmitGetClientInfoByName(std::string name, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetClientByName;
		req.query_name = OS::strip_whitespace(name);
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitClientMessage(int target_id, std::string message, EChatMessageType message_type, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SendClientMessage;
		req.query_data.client_info.client_id = target_id;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.message = message;
		req.message_type = message_type;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitChannelMessage(int target_id, std::string message, EChatMessageType message_type, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SendChannelMessage;
		req.query_data.channel_info.channel_id = target_id;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.message = message;
		req.message_type = message_type;
		getQueryTask()->AddRequest(req);
	}
	
	void ChatBackendTask::SubmitFind_OrCreateChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_Find_OrCreate_Channel;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_name = channel;
		getQueryTask()->AddRequest(req);	
	}
	void ChatBackendTask::SubmitFindChannel(ChatQueryCB cb, Peer *peer, void *extra, std::string channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_Find_Channel;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_name = channel;
		getQueryTask()->AddRequest(req);	
	}
	void ChatBackendTask::PerformFind_OrCreateChannel(ChatQueryRequest task_params, bool no_create) {
		struct Chat::_ChatQueryResponse resp;
		resp.channel_info =  GetChannelByName(task_params.query_name);

		if(resp.channel_info.channel_id == 0 && !no_create) {
			resp.channel_info = CreateChannel(task_params.query_name);
		}

		if(resp.channel_info.channel_id == 0) {
			resp.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, ""));
		}
		resp.client_info.client_id = 0;
		task_params.callback(task_params, resp, task_params.peer, task_params.extra);
	}
	void ChatBackendTask::PerformGetClientInfoByName(ChatQueryRequest task_params) {
		int client_id = 0;
		ChatClientInfo info;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET nick_id_map %s", task_params.query_name.c_str());

		if(reply->type == REDIS_REPLY_STRING) {
			client_id = atoi(OS::strip_quotes(reply->str).c_str());
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			client_id = reply->integer;
		}

		if(reply->type != REDIS_REPLY_NIL) {
			LoadClientInfoByID(info, client_id);
		}
		struct Chat::_ChatQueryResponse response;
		response.client_info = info;

		if(client_id == 0) {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, ""));
		}
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
		freeReplyObject(reply);
	}
	void ChatBackendTask::SubmitSetChannelKeys(ChatQueryCB cb, Peer *peer, void *extra, std::string channel, std::string user, const std::map<std::string, std::string> set_data_map) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SetChannelClientKeys;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;		
		req.query_data.channel_info.name = channel;
		req.query_data.client_info.name = user;
		req.set_keys = set_data_map;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::LoadClientInfoByID(ChatClientInfo &info, int client_id) {
		info.client_id = client_id;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));
		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d nick", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.name = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d user", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.user = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d realname", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.realname = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d host", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.hostname = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);


		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d ip", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.ip = OS::Address(OS::strip_quotes(std::string(reply->str)).c_str());
		}
		freeReplyObject(reply);

		info.profileid = 0;
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d profileid", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.profileid = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		info.operflags = 0;
		if(info.profileid != 0) {
			reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_opers %d", info.profileid);
			if(reply->type == REDIS_REPLY_STRING) {
				info.operflags = atoi(OS::strip_quotes(std::string(reply->str)).c_str());
			}
			freeReplyObject(reply);
		}

		reply = (redisReply *)redisCommand(mp_redis_connection, "HKEYS chat_client_%d_custkeys", client_id);
		for(unsigned int i=0;i<reply->elements;i++) {
			redisReply *key_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d_custkeys %s", client_id,OS::strip_quotes(reply->element[i]->str).c_str());
			if(key_reply && key_reply->type == REDIS_REPLY_INTEGER) {
				std::ostringstream s;
				s << key_reply->integer;
				info.custom_keys[OS::strip_quotes(reply->element[i]->str)] = s.str();
			} else if(key_reply && key_reply->type == REDIS_REPLY_STRING) {
				info.custom_keys[OS::strip_quotes(reply->element[i]->str)] = OS::strip_quotes(key_reply->str);
			}
			freeReplyObject(key_reply);
		}
		freeReplyObject(reply);

	}
	void ChatBackendTask::LoadClientInfoByName(ChatClientInfo &info, std::string name) {
		int client_id = 0;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET nick_id_map %s", name.c_str());
		if(reply->type == REDIS_REPLY_STRING) {
			client_id = atoi(reply->str);
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			client_id = reply->integer;
		}
		LoadClientInfoByID(info, client_id);
		freeReplyObject(reply);
	}
	void ChatBackendTask::PerformSendClientMessage(ChatQueryRequest task_params) {
		std::string kv_user = ClientInfoToKVString(task_params.peer->getClientInfo());
		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)task_params.message.c_str(),task_params.message.length());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\send_client_msg\\msg_type\\%d%s\\to_id\\%d\\msg\\%s\n",
			chat_messaging_channel, task_params.message_type, kv_user.c_str(), task_params.query_data.client_info.client_id, b64_msg));

		free((void *)b64_msg);
	}
	void ChatBackendTask::PerformSendChannelMessage(ChatQueryRequest task_params) {
		std::string kv_user = ClientInfoToKVString(task_params.peer->getClientInfo());
		ChatChannelInfo channel = GetChannelByID(task_params.query_data.channel_info.channel_id);
		std::string kv_chan = ChannelInfoToKVString(channel);
		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)task_params.message.c_str(),task_params.message.length());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\send_channel_msg\\msg_type\\%d%s%s\\msg\\%s\n",
			chat_messaging_channel, task_params.message_type, kv_user.c_str(), kv_chan.c_str(), b64_msg));

		free((void *)b64_msg);
	}
	void ChatBackendTask::SubmitClientInfo(ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_UpdateOrInsertClient;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitAddUserToChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_AddUserToChannel;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitRemoveUserFromChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, EChannelPartTypes reason, std::string reason_str) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_RemoveUserFromChannel;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		req.part_reason = reason;
		req.message = reason_str;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitGetChannelUsers(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetChannelUsers;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitGetChannelUser(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, std::string name) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetChannelUser;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info.name = name;
		req.query_data.client_info.client_id = 0;
		req.query_data.channel_info = channel;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitUpdateChannelModes(ChatQueryCB cb, Peer *peer, void *extra, uint32_t addmask, uint32_t removemask, ChatChannelInfo channel, std::string password, int limit, std::vector<std::pair<std::string, ChanClientModeChange> > user_modechanges) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_UpdateChannelModes;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		req.add_flags = addmask;
		req.remove_flags = removemask;
		req.chan_limit = limit;
		req.chan_password = password;
		req.user_modechanges = user_modechanges;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitSetChanProps(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredChanProps chanprops) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SaveChanProps;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.channel_props_data = chanprops;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitGetChanProps(ChatQueryCB cb, Peer *peer, void *extra, std::string mask) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetChanProps;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.channel_props_data.channel_mask = mask;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitDeleteChanProps(ChatQueryCB cb, Peer *peer, void *extra, int id) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_DeleteChanProps;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.channel_props_data.id = id;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformSetChanProps(ChatQueryRequest task_params) {
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		int id = task_params.query_data.channel_props_data.id;

		if(id == 0) {
			redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "INCR %s", mp_chanprops_pk_name);
			if(reply && reply->type == REDIS_REPLY_INTEGER) {
				id = reply->integer;
			} else if(reply && reply->type == REDIS_REPLY_STRING) {
				id = atoi(reply->str);
			}
		}
		std::string kv_string = ChanPropsToKVString(task_params.query_data.channel_props_data);

		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d id %d", id, id));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d channelmask %s", id, task_params.query_data.channel_props_data.channel_mask.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d comment %s", id, task_params.query_data.channel_props_data.comment.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d entrymsg %s", id, task_params.query_data.channel_props_data.entrymsg.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d setby %s", id, task_params.query_data.channel_props_data.setby.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d topic %s", id, task_params.query_data.channel_props_data.topic.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d password %s", id, task_params.query_data.channel_props_data.password.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d limit %d", id, task_params.query_data.channel_props_data.limit));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d seton %d", id, task_params.query_data.channel_props_data.seton));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d setbypid %d", id, task_params.query_data.channel_props_data.setbypid));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d expires %d", id, task_params.query_data.channel_props_data.expires));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_chanprops_%d modeflags %d", id, task_params.query_data.channel_props_data.modeflags));

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\save_chanprops%s%s\n",
			chat_messaging_channel, kv_string.c_str()));

	}
	void ChatBackendTask::PerformGetChanProps(ChatQueryRequest task_params) {
		ChatStoredUserMode usermode;
		int cursor = 0;

		int chanprops_id;
		struct Chat::_ChatQueryResponse response;
		redisReply *reply;
		ChatStoredChanProps chanprops;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		int add_count = 0;

		 std::vector<ChatStoredUserMode> matches;
		 do {
		 	reply = (redisReply *)redisCommand(mp_redis_connection, "SCAN %d MATCH chat_chanprops_*", cursor);
		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if(reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}
	 	
		 	for(int i=0;i<reply->element[1]->elements;i++) {
		 		redisReply *id_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s id", reply->element[1]->element[i]->str);
		 		if(id_reply->type == REDIS_REPLY_INTEGER) {
		 			chanprops_id = id_reply->integer;
		 		} else if(id_reply->type == REDIS_REPLY_STRING) {
		 			chanprops_id = atoi(id_reply->str);
		 		}
		 		freeReplyObject(id_reply);
		 		std::string channel_mask;
		 		id_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s channelmask", reply->element[1]->element[i]->str);
		 		if(id_reply->type == REDIS_REPLY_STRING) {
		 			channel_mask = id_reply->str;
			 	} else {
			 	}
			 	if(match(task_params.query_data.channel_props_data.channel_mask.c_str(), channel_mask.c_str()) == 0) {
			 		response.chanprops.push_back(GetChanPropsByID(chanprops_id));
			 		add_count++;
			 	}
		 		freeReplyObject(id_reply);
		 	}

		 	freeReplyObject(reply);
		 } while(cursor != 0);

		 if(add_count == 0) {
		 	response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoChanProps, "No such channel mode"));
		 }

		task_params.callback(task_params, response, task_params.peer, task_params.extra);		
	}
	void ChatBackendTask::PerformDeleteChanProps(ChatQueryRequest task_params) {
		struct Chat::_ChatQueryResponse response;
		ChatStoredChanProps chanprops = GetChanPropsByID(task_params.query_data.channel_props_data.id);
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));
		if(chanprops.id != 0) {
			freeReplyObject(redisCommand(mp_redis_connection, "DEL chat_chanprops_%d", chanprops.id));
		} else {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoChanProps, "No matching chanprops"));
		}
		
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}
	void ChatBackendTask::PerformGetChannelUser(ChatQueryRequest task_params) {
		struct Chat::_ChatQueryResponse response;
		ChatChannelInfo channel = GetChannelByID(task_params.query_data.channel_info.channel_id);
		
		ChatClientInfo client_info;

		if(task_params.query_data.client_info.client_id != 0) {
			LoadClientInfoByID(client_info, task_params.query_data.client_info.client_id);
		} else {
			LoadClientInfoByName(client_info, task_params.query_data.client_info.name);
		}

		ChatChanClientInfo chan_client = GetChanClientInfo(task_params.query_data.channel_info.channel_id,client_info.client_id);

		response.chan_client_info = chan_client;
		response.client_info = client_info;
		response.channel_info = channel;
		if(client_info.client_id == 0 || channel.channel_id == 0) {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, ""));
			goto end_error;
		}
		end_error:
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}
	void ChatBackendTask::PerformUpdateChannelModes(ChatQueryRequest task_params) {
		ChatChannelInfo channel_info;
		redisReply *reply;
		std::string chan_str;
		std::string user_str;
		struct Chat::_ChatQueryResponse response;
		int client_chan_flags = 0;
		int modeflags = 0;
		int old_modeflags;
		std::ostringstream pw_str;
		std::vector<std::pair<std::string, ChanClientModeChange> >::iterator user_modechanges_it = task_params.user_modechanges.begin();
		ChatClientInfo client_info;
		std::string from_user_str;
		ChatChanClientInfo chan_client_info;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));
		if(task_params.query_data.channel_info.channel_id != 0) {
			channel_info = GetChannelByID(task_params.query_data.channel_info.channel_id);
		} else {
			channel_info = GetChannelByName(task_params.query_data.channel_info.name);
		}

		if(channel_info.channel_id == 0) {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, ""));
			goto end_error;
		}

		response.channel_info = channel_info;
		
		
		chan_client_info = GetChanClientInfo(channel_info.channel_id, task_params.peer->getClientInfo().client_id);
		//test permissions

		if(!TestChannelPermissions(chan_client_info, channel_info, task_params, response)) {
			goto end_error;
		}

		//

		old_modeflags = channel_info.modeflags;

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d modeflags", channel_info.channel_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			modeflags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			modeflags = atoi(reply->str);
		}
		freeReplyObject(reply);

		modeflags |= task_params.add_flags;
		modeflags &= ~task_params.remove_flags;

		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d modeflags %d", channel_info.channel_id, modeflags));

		if(task_params.chan_password.length()) {
			freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d password \"%s\"", channel_info.channel_id, task_params.chan_password.c_str()));
		} else {
			freeReplyObject(redisCommand(mp_redis_connection, "HDEL chat_channel_%d password", channel_info.channel_id));
		}
		
		if(task_params.chan_limit == -1) {
			freeReplyObject(redisCommand(mp_redis_connection, "HDEL chat_channel_%d limit", channel_info.channel_id));			
		} else {
			freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d limit %d", channel_info.channel_id, task_params.chan_limit));
		}

		channel_info.modeflags = modeflags;
		response.channel_info = channel_info;

		chan_str = ChannelInfoToKVString(channel_info);
		user_str = ClientInfoToKVString(task_params.peer->getClientInfo());

		if(task_params.chan_password.length()) {
			pw_str << "\\new_password\\" << task_params.chan_password.c_str();
		}
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\channel_update_modeflags%s%s\\old_modeflags\\%d\\new_limit\\%d%s\n",
			chat_messaging_channel, chan_str.c_str(), user_str.c_str(), old_modeflags, task_params.chan_limit, pw_str.str().c_str()));

		while(user_modechanges_it != task_params.user_modechanges.end()) {
			std::pair<std::string, ChanClientModeChange> o = *user_modechanges_it;

			from_user_str = ClientInfoToKVString(task_params.peer->getClientInfo(), "from");
		
			LoadClientInfoByName(client_info, o.first);
			user_str = ClientInfoToKVString(client_info, "target");

			reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chan_%d_client_%d client_flags", channel_info.channel_id, task_params.query_data.client_info.client_id);
			if(reply && reply->type == REDIS_REPLY_INTEGER) {
				client_chan_flags = reply->integer;
			} else if(reply && reply->type == REDIS_REPLY_STRING) {
				client_chan_flags = atoi(reply->str);
			}
			freeReplyObject(reply);

			if(o.second.set) {
				client_chan_flags |= o.second.mode_flag;
			} else {
				client_chan_flags &= ~o.second.mode_flag;
			}
			freeReplyObject(redisCommand(mp_redis_connection, "HSET chan_%d_client_%d client_flags %d", channel_info.channel_id, task_params.query_data.client_info.client_id, client_chan_flags));

			freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\channel_update_usermode_flags%s%s%s\\modeflags\\%d\\set_mode\\%d\n",
				chat_messaging_channel, chan_str.c_str(), user_str.c_str(), from_user_str.c_str(), o.second.mode_flag, o.second.set));
			user_modechanges_it++;
		}

		end_error:

		task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}
	void ChatBackendTask::SubmitUpdateChannelTopic(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, std::string topic) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_UpdateChannelTopic;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		req.message = topic;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformUpdateChannelTopic(ChatQueryRequest task_params) {
		ChatChannelInfo channel_info;
		ChatClientInfo client_info;
		std::string chan_str;
		std::string user_str;
		struct Chat::_ChatQueryResponse response;
		ChatChanClientInfo chan_client_info;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		if(task_params.query_data.channel_info.channel_id != 0) {
			channel_info = GetChannelByID(task_params.query_data.channel_info.channel_id);
		} else {
			channel_info = GetChannelByName(task_params.query_data.channel_info.name);
		}

		client_info = task_params.peer->getClientInfo();

		if(channel_info.channel_id == 0) {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, ""));
			goto end_error;
		}

		chan_client_info = GetChanClientInfo(channel_info.channel_id, task_params.peer->getClientInfo().client_id);
		//test permissions
		if(!TestChannelPermissions(chan_client_info, channel_info, task_params, response)) {
			goto end_error;
		}
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic \"%s\"", channel_info.channel_id, task_params.message.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic_seton %d", channel_info.channel_id, time(NULL)));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic_setby %s", channel_info.channel_id, client_info.name.c_str()));

		channel_info.topic = task_params.message;
		
		response.channel_info = channel_info;

		chan_str = ChannelInfoToKVString(channel_info);
		user_str = ClientInfoToKVString(task_params.peer->getClientInfo());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\channel_update_topic\\%s%s\\\n",
			chat_messaging_channel, chan_str.c_str(), user_str.c_str()));

		end_error:

		task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}

	void ChatBackendTask::PerformSendAddUserToChannel(ChatQueryRequest task_params) {
		std::string chan_str = ChannelInfoToKVString(task_params.query_data.channel_info);
		std::string user_str = ClientInfoToKVString(task_params.peer->getClientInfo());

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		freeReplyObject(redisCommand(mp_redis_connection, "LPUSH chan_%d_clients %d", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));

		freeReplyObject(redisCommand(mp_redis_connection, "HSET chan_%d_client_%d client_flags 0", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\user_join_channel%s%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str()));

	}
	void ChatBackendTask::PerformSendRemoveUserFromChannel(ChatQueryRequest task_params) {
		std::string chan_str = ChannelInfoToKVString(task_params.query_data.channel_info);
		std::string user_str = ClientInfoToKVString(task_params.peer->getClientInfo());

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		freeReplyObject(redisCommand(mp_redis_connection, "LREM chan_%d_clients 0 %d", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));

		freeReplyObject(redisCommand(mp_redis_connection, "DEL chan_%d_client_%d", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));
		freeReplyObject(redisCommand(mp_redis_connection, "DEL chan_%d_client_%d_custkeys", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));

		
		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)task_params.message.c_str(),task_params.message.length());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\user_leave_channel%s%s\\part_type\\%d\\reason\\%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str(), task_params.part_reason, b64_msg));

		free((void *)b64_msg);
	}
	void ChatBackendTask::PerformUpdateOrInsertClient(ChatQueryRequest task_params) {
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		if(task_params.query_data.client_info.client_id == 0) {
			reply = (redisReply *)redisCommand(mp_redis_connection, "INCR %s", mp_pk_name);
			if(reply && reply->type == REDIS_REPLY_INTEGER) {
				task_params.query_data.client_info.client_id = reply->integer;
			} else if(reply && reply->type == REDIS_REPLY_STRING) {
				task_params.query_data.client_info.client_id = atoi(reply->str);
			}
			freeReplyObject(reply);
		}

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d nick \"%s\"", task_params.query_data.client_info.client_id,OS::strip_whitespace(task_params.query_data.client_info.name.c_str()).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d user \"%s\"", task_params.query_data.client_info.client_id,OS::strip_whitespace(task_params.query_data.client_info.user.c_str()).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d realname \"%s\"", task_params.query_data.client_info.client_id,OS::strip_whitespace(task_params.query_data.client_info.realname.c_str()).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d host \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.hostname.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d ip \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.ip.ToString().c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d profileid \"%d\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.profileid);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET nick_id_map %s %d", task_params.query_data.client_info.name.c_str(), task_params.query_data.client_info.client_id);
		freeReplyObject(reply);

		struct Chat::_ChatQueryResponse response;
		response.client_info = task_params.query_data.client_info;

		if(task_params.callback)
			task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}
	void ChatBackendTask::PerformSetClientKeys(ChatQueryRequest task_params) {
		redisReply *reply;
		std::map<std::string, std::string>::iterator it = task_params.set_keys.begin();
		ChatClientInfo client_info;
		std::string set_kv_str;

		if(task_params.query_data.client_info.client_id != 0) {
			LoadClientInfoByID(client_info, task_params.query_data.client_info.client_id);
		} else {
			LoadClientInfoByName(client_info, task_params.query_data.client_info.name);
		}

		while(it != task_params.set_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(p.second.length()) {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d_custkeys %s \"%s\"", client_info.client_id, p.first.c_str(),OS::strip_whitespace(p.second.c_str()).c_str());
			} else {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HDEL chat_client_%d_custkeys %s", client_info.client_id, p.first.c_str());
			}
			freeReplyObject(reply);
			it++;
		}
	}
	void ChatBackendTask::SubmitSetClientKeys(ChatQueryCB cb, Peer *peer, void *extra, int client_id, const std::map<std::string, std::string> set_data_map) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SetClientKeys;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info.client_id = client_id;
		req.set_keys = set_data_map;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitClientDelete(ChatQueryCB cb, Peer *peer, void *extra, std::string reason) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_UserDelete;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.message = reason;

		peer->GetChannelList(req.id_list);
		req.client_info = peer->getClientInfo();
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformUserDelete(ChatQueryRequest task_params) {
		int client_id = task_params.client_info.client_id;
		std::vector<int>::iterator chan_it = task_params.id_list.begin();
		std::string user_str = ClientInfoToKVString(task_params.client_info);

		while(chan_it != task_params.id_list.end()) {
			int chan_id = *chan_it;
			freeReplyObject(redisCommand(mp_redis_connection, "LREM chan_%d_clients 0 %d", chan_id, client_id));

			freeReplyObject(redisCommand(mp_redis_connection, "DEL chan_%d_client_%d", chan_id, client_id));
			freeReplyObject(redisCommand(mp_redis_connection, "DEL chan_%d_client_%d_custkeys", chan_id, client_id));
			chan_it++;
		}

		freeReplyObject(redisCommand(mp_redis_connection, "DEL chat_client_%d", client_id));
		freeReplyObject(redisCommand(mp_redis_connection, "HDEL nick_id_map \"%s\"", task_params.peer->getClientInfo().name.c_str()));

		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)task_params.message.c_str(),task_params.message.length());

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\user_quit%s\\reason\\%s\n",
			chat_messaging_channel, user_str.c_str(), b64_msg));

		free((void *)b64_msg);
	}
	void ChatBackendTask::SubmitSetChannelKeys(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel, const std::map<std::string, std::string> set_data_map) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SetChannelKeys;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.channel_info = channel;
		req.set_keys = set_data_map;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformSetChannelKeys(ChatQueryRequest task_params) {
		redisReply *reply;
		std::map<std::string, std::string>::iterator it = task_params.set_keys.begin();
		std::string set_kv_str;

		std::string chan_str = ChannelInfoToKVString(task_params.query_data.channel_info);
		std::string user_str = ClientInfoToKVString(task_params.peer->getClientInfo());

		ChatChannelInfo channel_info;
		const char *b64_msg = NULL;
		struct Chat::_ChatQueryResponse response;
		std::pair<std::string, std::string> p;
		ChatChanClientInfo chan_client_info;

		if(task_params.query_data.channel_info.channel_id != 0) {
			channel_info = GetChannelByID(task_params.query_data.channel_info.channel_id);
		} else {
			channel_info = GetChannelByName(task_params.query_data.channel_info.name);
		}


		
		chan_client_info = GetChanClientInfo(channel_info.channel_id, task_params.peer->getClientInfo().client_id);
		//test permissions
		if(!TestChannelPermissions(chan_client_info, channel_info, task_params, response)) {
			goto end_error;
		}

		while(it != task_params.set_keys.end()) {
			p = *it;
			if(p.second.length()) {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_channel_%d_custkeys %s \"%s\"", channel_info.channel_id, p.first.c_str(),OS::strip_whitespace(p.second.c_str()).c_str());
			} else {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HDEL chat_channel_%d_custkeys %s", channel_info.channel_id, p.first.c_str());
			}
			freeReplyObject(reply);
			it++;
		}
		set_kv_str = OS::MapToKVString(task_params.set_keys);

		b64_msg = OS::BinToBase64Str((const uint8_t*)set_kv_str.c_str(),set_kv_str.length());

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\set_channel_keys%s%s\\key_data\\%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str(), b64_msg));

		end_error:
			if(b64_msg != NULL)
				free((void *)b64_msg);

		if(task_params.callback) {
			task_params.callback(task_params, response, task_params.peer, task_params.extra);
		}
	}
	void ChatBackendTask::SubmitGetChatOperFlags(int profileid, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetChatOperFlags;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info.profileid = profileid;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformGetChatOperFlags(ChatQueryRequest task_params) {
		struct Chat::_ChatQueryResponse resp;
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_opers %d", task_params.query_data.client_info.profileid);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			resp.operflags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			resp.operflags = atoi(reply->str);
		} else {
			resp.operflags = 0; //for user is not in channel check
		}

		freeReplyObject(reply);
		task_params.callback(task_params, resp, task_params.peer, task_params.extra);
	}
	ChatChanClientInfo ChatBackendTask::GetChanClientInfo(int chan_id, int client_id) {
		ChatChanClientInfo ret;
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		ret.client_id = client_id;

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chan_%d_client_%d client_flags", chan_id, client_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			ret.client_flags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			ret.client_flags = atoi(reply->str);
		} else {
			ret.client_id = 0; //for user is not in channel check
		}

		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HKEYS chan_%d_client_%d_custkeys", chan_id, client_id);
		for(unsigned int i=0;i<reply->elements;i++) {
			redisReply *key_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chan_%d_client_%d_custkeys %s", chan_id, client_id,OS::strip_quotes(reply->element[i]->str).c_str());
			if(key_reply && key_reply->type == REDIS_REPLY_INTEGER) {
				std::ostringstream s;
				s << key_reply->integer;
				ret.custom_keys[OS::strip_quotes(reply->element[i]->str)] = s.str();
			} else if(key_reply && key_reply->type == REDIS_REPLY_STRING) {
				ret.custom_keys[OS::strip_quotes(reply->element[i]->str)] = OS::strip_quotes(key_reply->str);
			}
			freeReplyObject(key_reply);
		}
		freeReplyObject(reply);

		LoadClientInfoByID(ret.client_info, client_id);

		return ret;
	}
	void ChatBackendTask::PerformGetChannelUsers(ChatQueryRequest task_params) {
		redisReply *reply;
		int num_clients = 0;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		reply = (redisReply *)redisCommand(mp_redis_connection, "LLEN chan_%d_clients", task_params.query_data.channel_info.channel_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			num_clients = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			num_clients = atoi(reply->str);
		}

		reply = (redisReply *)redisCommand(mp_redis_connection, "LRANGE chan_%d_clients 0 %d", task_params.query_data.channel_info.channel_id, num_clients);

		struct Chat::_ChatQueryResponse response;
		for(unsigned int i=0;i<reply->elements;i++) {
			redisReply *element = reply->element[i];
			int client_id;
			if(element && element->type == REDIS_REPLY_INTEGER) {
				client_id = element->integer;
			} else if(element && element->type == REDIS_REPLY_STRING) {
				client_id = atoi(element->str);
			}
			response.m_channel_clients.push_back(GetChanClientInfo(task_params.query_data.channel_info.channel_id,client_id));
		}
		freeReplyObject(reply);
		task_params.callback(task_params, response, task_params.peer, task_params.extra);

	}
	void ChatBackendTask::PerformSetChannelClientKeys(ChatQueryRequest task_params) {
		redisReply *reply;
		std::map<std::string, std::string>::iterator it = task_params.set_keys.begin();
		ChatChannelInfo channel = GetChannelByName(task_params.query_data.channel_info.name);
		ChatClientInfo client;
		std::string set_kv_str;
		const char *b64_msg;
		LoadClientInfoByName(client, task_params.query_data.client_info.name);

		std::string chan_str = ChannelInfoToKVString(channel);
		std::string user_str = ClientInfoToKVString(client);

		while(it != task_params.set_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(p.second.length()) {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chan_%d_client_%d_custkeys %s \"%s\"", channel.channel_id, client.client_id, p.first.c_str(),OS::strip_whitespace(p.second.c_str()).c_str());
			} else {
				reply = (redisReply *)redisCommand(mp_redis_connection, "HDEL chan_%d_client_%d_custkeys %s", channel.channel_id, client.client_id, p.first.c_str());
			}
			freeReplyObject(reply);
			it++;
		}
		set_kv_str = OS::MapToKVString(task_params.set_keys);

		b64_msg = OS::BinToBase64Str((const uint8_t*)set_kv_str.c_str(),set_kv_str.length());

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\set_channel_client_keys%s%s\\key_data\\%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str(), b64_msg));

		free((void *)b64_msg);
	}
	void ChatBackendTask::SubmitSetSavedUserMode(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredUserMode usermode) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SaveUserMode;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.usermode_data = usermode;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitGetSavedUserModes(ChatQueryCB cb, Peer *peer, void *extra, ChatStoredUserMode usermode) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetUserModes;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.usermode_data = usermode;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformSaveUserMode(ChatQueryRequest task_params) {
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		int id = task_params.query_data.usermode_data.id;
		std::string kv_usermode = UsermodeToKVString(task_params.query_data.usermode_data);

		if(id == 0) {
			redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "INCR %s", mp_usermode_pk_name);
			if(reply && reply->type == REDIS_REPLY_INTEGER) {
				id = reply->integer;
			} else if(reply && reply->type == REDIS_REPLY_STRING) {
				id = atoi(reply->str);
			}
		}

		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d id %d", id, id));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d hostmask %s", id, OS::strip_whitespace(task_params.query_data.usermode_data.hostmask).c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d chanmask %s", id, OS::strip_whitespace(task_params.query_data.usermode_data.chanmask).c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d comment %s", id, task_params.query_data.usermode_data.comment.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d setby %s", id, task_params.query_data.usermode_data.setby.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d machineid %s", id, task_params.query_data.usermode_data.machineid.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d profileid %d", id, task_params.query_data.usermode_data.profileid));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d setbypid %d", id, task_params.query_data.usermode_data.setbypid));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_usermode_%d modeflags %d", id, task_params.query_data.usermode_data.modeflags));

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\save_usermode%s%s\n",
			chat_messaging_channel, kv_usermode.c_str()));
	}
	void ChatBackendTask::PerformGetUserModes(ChatQueryRequest task_params) {
		ChatStoredUserMode usermode;
		int cursor = 0;

		int usermode_id;
		struct Chat::_ChatQueryResponse response;
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		 std::vector<ChatStoredUserMode> matches;
		 do {
		 	reply = (redisReply *)redisCommand(mp_redis_connection, "SCAN %d MATCH chat_usermode_*", cursor);
		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if(reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}
	 	
		 	for(int i=0;i<reply->element[1]->elements;i++) {
		 		redisReply *id_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s id", reply->element[1]->element[i]->str);
		 		if(id_reply->type == REDIS_REPLY_INTEGER) {
		 			usermode_id = id_reply->integer;
		 		} else if(id_reply->type == REDIS_REPLY_STRING) {
		 			usermode_id = atoi(id_reply->str);
		 		}
		 		usermode = GetUserModeByID(usermode_id);
		 		if(match(task_params.query_data.usermode_data.chanmask.c_str(), usermode.chanmask.c_str()) == 0) {
		 			response.usermodes.push_back(usermode);
		 		}
		 		freeReplyObject(id_reply);
		 	}

		 	freeReplyObject(reply);
		 } while(cursor != 0);

		task_params.callback(task_params, response, task_params.peer, task_params.extra);		
	}
	void ChatBackendTask::SubmitDeleteSavedUserMode(ChatQueryCB cb, Peer *peer, void *extra, int id) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_DeleteUserMode;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.usermode_data.id = id;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::PerformDeleteUserMode(ChatQueryRequest task_params) {
		struct Chat::_ChatQueryResponse response;
		ChatStoredUserMode usermode = GetUserModeByID(task_params.query_data.usermode_data.id);
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));
		if(usermode.id != 0) {
			freeReplyObject(redisCommand(mp_redis_connection, "DEL chat_usermode_%d", task_params.query_data.usermode_data.id));
		} else {
			response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUserModeID, "No such usermode"));
		}
		
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
	}
	ChatStoredUserMode ChatBackendTask::GetUserModeByID(int id) {
		ChatStoredUserMode usermode;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d id", id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			usermode.id = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.id = atoi(reply->str);
		} else {
			usermode.id = 0;
		}
		freeReplyObject(reply);


		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d hostmask", id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.hostmask = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d chanmask", id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.chanmask = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d comment", id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.comment = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d setby", id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.setby = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d machineid", id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.machineid = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d profileid", id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			usermode.profileid = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.profileid = atoi(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d setbypid", id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			usermode.setbypid = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.setbypid = atoi(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_usermode_%d modeflags", id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			usermode.modeflags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			usermode.modeflags = atoi(reply->str);
		}
		freeReplyObject(reply);

		return usermode;
	}
	ChatStoredChanProps ChatBackendTask::GetChanPropsByID(int chanprops_id) {
		ChatStoredChanProps props;
		props.id = 0;

		redisReply *reply;

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d id", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.id = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.id = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d modeflags", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.modeflags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.modeflags = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d limit", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.limit = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.limit = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d seton", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.seton = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.seton = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d setbypid", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.setbypid = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.setbypid = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d expires", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			props.expires = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			props.expires = atoi(OS::strip_quotes(reply->str).c_str());
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d topic", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			props.topic = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d entrymsg", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			props.entrymsg = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d setby", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			props.setby = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d password", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			props.password = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_chanprops_%d mask", chanprops_id);
		if(reply && reply->type == REDIS_REPLY_STRING) {
			props.channel_mask = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		return props;
	}
	ChatChannelInfo ChatBackendTask::GetChannelByName(std::string name) {
		int id = 0;
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chan_id_map %s",name.c_str());

		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			id = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			id = atoi(reply->str);
		}
		freeReplyObject(reply);

		return GetChannelByID(id);

	}
	ChatChannelInfo ChatBackendTask::GetChannelByID(int id) {
		ChatChannelInfo ret;
		ret.channel_id = id;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d topic", id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.topic = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		ret.modeflags = 0;
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d modeflags", ret.channel_id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.modeflags = atoi(OS::strip_quotes(reply->str).c_str());
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			ret.modeflags = reply->integer;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d limit", ret.channel_id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.limit = atoi(OS::strip_quotes(reply->str).c_str());
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			ret.limit = reply->integer;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d password", ret.channel_id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.password = OS::strip_quotes(reply->str).c_str();
		}
		freeReplyObject(reply);

		ret.topic_seton = 0;
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d topic_seton", ret.channel_id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.topic_seton = atoi(OS::strip_quotes(reply->str).c_str());
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			ret.topic_seton = reply->integer;
		}
		freeReplyObject(reply);

		ret.topic_setby = "SERVER!SERVER@*";
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d topic_setby", ret.channel_id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.topic_setby = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d name", id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.name = OS::strip_quotes(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HKEYS chat_channel_%d_custkeys", id);
		for(unsigned int i=0;i<reply->elements;i++) {
			redisReply *key_reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d_custkeys %s", id,OS::strip_quotes(reply->element[i]->str).c_str());
			if(key_reply && key_reply->type == REDIS_REPLY_INTEGER) {
				std::ostringstream s;
				s << key_reply->integer;
				ret.custom_keys[OS::strip_quotes(reply->element[i]->str)] = s.str();
			} else if(key_reply && key_reply->type == REDIS_REPLY_STRING) {
				ret.custom_keys[OS::strip_quotes(reply->element[i]->str)] = OS::strip_quotes(key_reply->str);
			}
			freeReplyObject(key_reply);
		}

		return ret;
	}
	ChatChannelInfo ChatBackendTask::CreateChannel(std::string name) {
		ChatChannelInfo ret;
		redisReply *reply;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		reply = (redisReply *)redisCommand(mp_redis_connection, "INCR %s", mp_chan_pk_name);
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			ret.channel_id = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			ret.channel_id = atoi(reply->str);
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chan_id_map %s %d", name.c_str(), ret.channel_id);
		freeReplyObject(reply);

		ret.name = name;
		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_channel_%d name %s", ret.channel_id, name.c_str());
		freeReplyObject(reply);

		ret.topic = "Default Topic";
		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_channel_%d topic \"Default Topic\"", ret.channel_id);
		freeReplyObject(reply);

		ret.topic_seton = 666;
		ret.topic_setby = "SERVER!SERVER@*";

		ret.modeflags = 0;
		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_channel_%d modeflags 0", ret.channel_id);
		freeReplyObject(reply);

		return ret;
	}
	void *ChatBackendTask::TaskThread(OS::CThread *thread) {
		ChatBackendTask *task = (ChatBackendTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<ChatQueryRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					ChatQueryRequest task_params = *it;
					it = task->m_request_list.erase(it);
					switch(task_params.type) {
						case EChatQueryRequestType_GetClientByName:
							task->PerformGetClientInfoByName(task_params);
						break;
						case EChatQueryRequestType_UpdateOrInsertClient:
							task->PerformUpdateOrInsertClient(task_params);
						break;
						case EChatQueryRequestType_SendClientMessage:
							task->PerformSendClientMessage(task_params);
						break;
						case EChatQueryRequestType_SendChannelMessage:
							task->PerformSendChannelMessage(task_params);
						break;
						case EChatQueryRequestType_Find_OrCreate_Channel:
							task->PerformFind_OrCreateChannel(task_params);
						break;
						case EChatQueryRequestType_Find_Channel:
							task->PerformFind_OrCreateChannel(task_params, true);
						break;
						case EChatQueryRequestType_AddUserToChannel:
							task->PerformSendAddUserToChannel(task_params);
						break;
						case EChatQueryRequestType_RemoveUserFromChannel:
							task->PerformSendRemoveUserFromChannel(task_params);
						break;
						case EChatQueryRequestType_GetChannelUsers:
							task->PerformGetChannelUsers(task_params);
						break;
						case EChatQueryRequestType_GetChannelUser:
							task->PerformGetChannelUser(task_params);
						break;
						case EChatQueryRequestType_UpdateChannelModes:
							task->PerformUpdateChannelModes(task_params);
						break;
						case EChatQueryRequestType_UpdateChannelTopic:
							task->PerformUpdateChannelTopic(task_params);
						break;
						case EChatQueryRequestType_SetChannelClientKeys:
							task->PerformSetChannelClientKeys(task_params);
						break;
						case EChatQueryRequestType_SetClientKeys:
							task->PerformSetClientKeys(task_params);
						break;
						case EChatQueryRequestType_SetChannelKeys:
							task->PerformSetChannelKeys(task_params);
						break;
						case EChatQueryRequestType_UserDelete:
							task->PerformUserDelete(task_params);
						break;
						case EChatQueryRequestType_GetChatOperFlags:
							task->PerformGetChatOperFlags(task_params);
						break;
						case EChatQueryRequestType_GetUserModes:
							task->PerformGetUserModes(task_params);
						break;
						case EChatQueryRequestType_SaveUserMode:
							task->PerformSaveUserMode(task_params);
						break;
						case EChatQueryRequestType_DeleteUserMode:
							task->PerformDeleteUserMode(task_params);
						break;
						case EChatQueryRequestType_SaveChanProps:
							task->PerformSetChanProps(task_params);
						break;
						case EChatQueryRequestType_GetChanProps:
							task->PerformGetChanProps(task_params);
						break;
						case EChatQueryRequestType_DeleteChanProps:
							task->PerformDeleteChanProps(task_params);
						break;
					}
					if(task->m_flag_push_task) {
						task->m_flag_push_task = false;
						it = task->m_request_list.begin();
					}
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(CHAT_BACKEND_TICK);
		}
		return NULL;
	}

	void *ChatBackendTask::setup_redis_async(OS::CThread *thread) {
		ChatBackendTask *task = (ChatBackendTask *)thread->getParams();
		task->mp_event_base = event_base_new();

	    task->mp_redis_async_connection = redisAsyncConnect("127.0.0.1", 6379);
	    
	    redisLibeventAttach(task->mp_redis_async_connection, task->mp_event_base);

	    redisAsyncCommand(task->mp_redis_async_connection, onRedisMessage, task, "SUBSCRIBE %s",chat_messaging_channel);

	    event_base_dispatch(task->mp_event_base);
		return NULL;
	}


	void ChatBackendTask::onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
	    redisReply *r = (redisReply*)reply;
	    ChatBackendTask *task = (ChatBackendTask *)privdata;
	    if (reply == NULL) return;
		ChatChannelInfo channel_info;
		ChatClientInfo client_info;
		ChatClientInfo target_client_info;

		uint8_t *out;
		int len;
	    int type;
	    std::string msg_type;
	    std::string msg;
	    std::map<std::string,std::string> key_data;
	    ChanModeChangeData change_data;

	    

	    EChatMessageType chat_msg_type;
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		OS::KVReader kv_parser(r->element[2]->str);
	    		if(strcmp(r->element[1]->str,chat_messaging_channel) == 0) {
	    			type = kv_parser.GetValueInt("msg_type");
	    			if(!kv_parser.HasKey("type")) {
	    				return;
	    			}
	    			msg_type = kv_parser.GetValue("type");
					if(strcmp(msg_type.c_str(), "send_client_msg") == 0) {
		    			if(!kv_parser.HasKey("msg")) {
		    				return;
		    			}
		    			msg = kv_parser.GetValue("msg");
		    			chat_msg_type = (EChatMessageType)kv_parser.GetValueInt("msg_type");
		    			int target_id = kv_parser.GetValueInt("to_id");
						ChatClientInfo user = ClientInfoFromKVString(r->element[2]->str);
						OS::Base64StrToBin(msg.c_str(), &out, len);
						task->SendClientMessageToDrivers(target_id,user, (const char *)out, chat_msg_type);
						free((void *)out);
					} else if(strcmp(msg_type.c_str(), "send_channel_msg") == 0) {
		    			if(!kv_parser.HasKey("msg")) {
		    				return;
		    			}
		    			msg = kv_parser.GetValue("msg");
		    			chat_msg_type = (EChatMessageType)kv_parser.GetValueInt("msg_type");
						ChatClientInfo user = ClientInfoFromKVString(r->element[2]->str);
						ChatChannelInfo channel = ChannelInfoFromKVString(r->element[2]->str);
						OS::Base64StrToBin(msg.c_str(), &out, len);
						task->SendChannelMessageToDrivers(channel,user, (const char *)out, (EChatMessageType)chat_msg_type);
						free((void *)out);
					} else if(strcmp(msg_type.c_str(), "user_join_channel") == 0) {
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						task->SendClientJoinChannelToDrivers(client_info, channel_info);
					} else if(strcmp(msg_type.c_str(), "user_leave_channel") == 0) {
						EChannelPartTypes part_reason = (EChannelPartTypes)kv_parser.GetValueInt("part_type");
						msg = kv_parser.GetValue("reason");
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						OS::Base64StrToBin(msg.c_str(), &out, len);
						task->SendClientPartChannelToDrivers(client_info, channel_info, part_reason, (const char *)out);
						free((void *)out);
					} else if(strcmp(msg_type.c_str(), "channel_update_modeflags") == 0) {
						if(kv_parser.HasKey("new_password")) {
							change_data.new_password = kv_parser.GetValue("new_password");
						}
						change_data.old_modeflags = kv_parser.GetValueInt("old_modeflags");
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						change_data.new_limit = kv_parser.GetValueInt("new_limit");

						task->SendChannelModeUpdateToDrivers(client_info, channel_info, change_data);
					} else if (strcmp(msg_type.c_str(), "channel_update_usermode_flags") == 0) {
						type = kv_parser.GetValueInt("modeflags");
						target_client_info = ClientInfoFromKVString(r->element[2]->str, "target");
						client_info = ClientInfoFromKVString(r->element[2]->str, "from");
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						ChanClientModeChange client_mode_info;
						client_mode_info.mode_flag = (EChanClientFlags)type;
						client_mode_info.client_info = target_client_info;
						client_mode_info.set = kv_parser.GetValueInt("set_mode");

						change_data.client_modechanges.push_back(client_mode_info);

						task->SendChannelModeUpdateToDrivers(client_info, channel_info, change_data);

					} else if(strcmp(msg_type.c_str(), "channel_update_topic") == 0) {
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						task->SendUpdateChannelTopicToDrivers(client_info, channel_info);
					} else if(strcmp(msg_type.c_str(), "set_channel_client_keys") == 0) {
		    			if(!kv_parser.HasKey("key_data")) {
		    				return;
		    			}
		    			msg = kv_parser.GetValue("key_data");
						
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						OS::Base64StrToBin(msg.c_str(), &out, len);

						key_data = OS::KeyStringToMap(std::string((const char *)out));

						task->SendSetChannelClientKeysToDrivers(client_info, channel_info, key_data);

						free((void *)out);
					} else if(strcmp(msg_type.c_str(), "set_channel_keys") == 0) {
		    			if(!kv_parser.HasKey("key_data")) {
		    				return;
		    			}
		    			msg = kv_parser.GetValue("key_data");
						
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						OS::Base64StrToBin(msg.c_str(), &out, len);

						key_data = OS::KeyStringToMap(std::string((const char *)out));

						task->SendSetChannelKeysToDrivers(client_info, channel_info, key_data);

						free((void *)out);
					} else if(strcmp(msg_type.c_str(), "user_quit") == 0) {
		    			if(!kv_parser.HasKey("reason")) {
		    				return;
		    			}
		    			msg = kv_parser.GetValue("reason");
						client_info = ClientInfoFromKVString(r->element[2]->str);

						OS::Base64StrToBin(msg.c_str(), &out, len);
						task->SendUserQuitMessage(client_info, (const char *)out);
						free((void *)out);
					}
	    		}
	    	}
	    }
	}
	std::string ChatBackendTask::ClientInfoToKVString(ChatClientInfo info, std::string prefix) {
		std::ostringstream s;
		s << "\\" << prefix << "client_nick\\" << info.name;
		s << "\\" << prefix << "client_user\\" << info.user;
		s << "\\" << prefix << "client_realname\\" << info.realname;
		s << "\\" << prefix << "client_host\\" << info.hostname;
		s << "\\" << prefix << "client_ip\\" << info.ip.ToString();
		s << "\\" << prefix << "client_id\\" << info.client_id;
		s << "\\" << prefix << "client_profileid\\" << info.profileid;
		s << "\\" << prefix << "client_operflags\\" << info.operflags;
		return s.str();
	}
	ChatClientInfo ChatBackendTask::ClientInfoFromKVString(const char *str, std::string prefix) {
		ChatClientInfo ret;
		std::ostringstream s;
		s << prefix << "client_nick";
		OS::KVReader kv_parser(str);

		if(kv_parser.HasKey(s.str())) {
			ret.name = kv_parser.GetValue(s.str());
		}
		s.str("");

		s << prefix << "client_user";
		if(kv_parser.HasKey(s.str())) {
			ret.user = kv_parser.GetValue(s.str());
		}
		s.str("");

		s << prefix << "client_realname";
		if(kv_parser.HasKey(s.str())) {
			ret.realname = kv_parser.GetValue(s.str());
		}
		s.str("");

		s << prefix << "client_host";
		if(kv_parser.HasKey(s.str())) {
			ret.hostname = kv_parser.GetValue(s.str());
		}
		s.str("");

		s << prefix << "client_ip";
		if(kv_parser.HasKey(s.str())) {
			ret.ip = OS::Address(s.str().c_str());
		}
		s.str("");

		s << prefix << "client_id";
		if(kv_parser.HasKey(s.str())) {
			ret.client_id = kv_parser.GetValueInt(s.str());
		}

		s << prefix << "client_profileid";
		if(kv_parser.HasKey(s.str())) {
			ret.profileid = kv_parser.GetValueInt(s.str());
		}

		s << prefix << "client_operflags";
		if(kv_parser.HasKey(s.str())) {
			ret.operflags = kv_parser.GetValueInt(s.str());
		}		

		return ret;
	}
	std::string ChatBackendTask::ChannelInfoToKVString(ChatChannelInfo info) {
		std::ostringstream s;
		s << "\\channel_name\\" << info.name;
		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)info.topic.c_str(),info.topic.length());
		s << "\\channel_topic_message\\" << b64_msg;
		free((void *)b64_msg);
		s << "\\channel_topic_seton\\" << info.topic_seton;
		s << "\\channel_topic_setby\\" << info.topic_setby;
		s << "\\channel_id\\" << info.channel_id;
		s << "\\channel_modeflags\\" << info.modeflags;
		s << "\\channel_limit\\" << info.limit;
		s << "\\channel_password\\" << info.password;
		return s.str();
	}
	ChatChannelInfo ChatBackendTask::ChannelInfoFromKVString(const char *str) {
		ChatChannelInfo ret;
		OS::KVReader kv_parser(str);

		if(kv_parser.HasKey("channel_name")) {
			ret.name = kv_parser.GetValue("channel_name");
		}

		if(kv_parser.HasKey("channel_topic_message")) {
			uint8_t *out;
			int len;
			OS::Base64StrToBin(kv_parser.GetValue("channel_topic_message").c_str(), &out, len);
			ret.topic = (const char *)out;
			free((void *)out);
		}
		if(kv_parser.HasKey("channel_topic_setby")) {
			ret.topic_setby = kv_parser.GetValue("channel_topic_setby");
		}

		if(kv_parser.HasKey("channel_topic_seton")) {
			ret.topic_seton = kv_parser.GetValueInt("channel_topic_seton");
		}

		if(kv_parser.HasKey("channel_id")) {

			ret.channel_id = kv_parser.GetValueInt("channel_id");
		}

		if(kv_parser.HasKey("channel_modeflags")) {
			ret.modeflags = kv_parser.GetValueInt("channel_modeflags");
		}

		if(kv_parser.HasKey("channel_limit")) {
			ret.limit = kv_parser.GetValueInt("channel_limit");
		} else {
			ret.limit = -1;
		}
		if(kv_parser.HasKey("channel_password")) {
			ret.password = kv_parser.GetValue("channel_password");
		}
		return ret;
	}
	std::string ChatBackendTask::UsermodeToKVString(ChatStoredUserMode info) {
		std::ostringstream s;
		s << "\\usermode_id\\" << info.id;
		s << "\\usermode_hostmask\\" << info.hostmask;
		s << "\\usermode_chanmask\\" << info.chanmask;
		s << "\\usermode_comment\\" << info.comment;
		if(info.machineid.length())
			s << "\\usermode_machineid\\" << info.machineid;
		if(info.setby.length())
			s << "\\usermode_setby\\" << info.setby;
		if(info.profileid != 0)
			s << "\\usermode_profileid\\" << info.profileid;

		s << "\\usermode_modeflags\\" << info.modeflags;

		return s.str();
	}
	ChatStoredUserMode ChatBackendTask::UsermodeFromKVString(const char *str) {
		ChatStoredUserMode ret;
		OS::KVReader kv_parser(str);

		if(kv_parser.HasKey("usermode_id")) {
			ret.id = kv_parser.GetValueInt("usermode_id");
		} else {
			ret.id = 0;
		}

		if(kv_parser.HasKey("usermode_comment")) {
			ret.comment = kv_parser.GetValue("usermode_comment");
		}

		if(kv_parser.HasKey("usermode_hostmask")) {
			ret.hostmask = kv_parser.GetValue("usermode_hostmask");
		}

		if(kv_parser.HasKey("usermode_chanmask")) {
			ret.chanmask = kv_parser.GetValue("usermode_chanmask");
		}

		if(kv_parser.HasKey("usermode_modeflags")) {
			ret.modeflags = kv_parser.GetValueInt("usermode_modeflags");
		} else {
			ret.modeflags = 0;
		}

		if(kv_parser.HasKey("usermode_profileid")) {
			ret.profileid = kv_parser.GetValueInt("usermode_profileid");
		} else {
			ret.profileid = 0;
		}

		if(kv_parser.HasKey("usermode_setbypid")) {
			ret.setbypid = kv_parser.GetValueInt("usermode_setbypid");
		} else {
			ret.setbypid = 0;
		}

		if(kv_parser.HasKey("usermode_machineid")) {
			ret.machineid = kv_parser.GetValue("usermode_machineid");
		}
		if(kv_parser.HasKey("usermode_setby")) {
			ret.setby = kv_parser.GetValue("usermode_setby");
		}

		return ret;
	}
	std::string ChatBackendTask::ChanPropsToKVString(ChatStoredChanProps info) {
		std::ostringstream s;
		s << "\\chanprops_id\\" << info.id;
		s << "\\chanprops_mask\\" << info.channel_mask;
		s << "\\chanprops_modeflags\\" << info.modeflags;
		s << "\\chanprops_comment\\" << info.comment;
		s << "\\chanprops_topic\\" << info.topic;
		s << "\\chanprops_entrymsg\\" << info.entrymsg;
		s << "\\chanprops_setby\\" << info.setby;
		s << "\\chanprops_seton\\" << info.seton;
		s << "\\chanprops_setbypid\\" << info.setbypid;
		s << "\\chanprops_limit\\" << info.limit;
		s << "\\chanprops_password\\" << info.password;
		s << "\\chanprops_expires\\" << info.expires;
		return s.str();
	}
	ChatStoredChanProps ChatBackendTask::ChanPropsFromKVString(const char *str) {
		ChatStoredChanProps ret;
		OS::KVReader kv_parser(str);

		if(kv_parser.HasKey("chanprops_id")) {
			ret.id = kv_parser.GetValueInt("chanprops_id");
		} else {
			ret.id = 0;
		}

		if(kv_parser.HasKey("chanprops_mask")) {
			ret.password = kv_parser.GetValue("chanprops_mask");
		}
		
		if(kv_parser.HasKey("chanprops_modeflags")) {
			ret.modeflags = kv_parser.GetValueInt("chanprops_modeflags");
		} else {
			ret.modeflags = 0;
		}

		if(kv_parser.HasKey("chanprops_seton")) {
			ret.seton = kv_parser.GetValueInt("chanprops_seton");
		} else {
			ret.seton = 0;
		}

		if(kv_parser.HasKey("chanprops_comment")) {
			ret.comment = kv_parser.GetValue("chanprops_comment");
		}
		if(kv_parser.HasKey("chanprops_entrymsg")) {
			ret.entrymsg = kv_parser.GetValue("chanprops_entrymsg");
		}
		if(kv_parser.HasKey("chanprops_topic")) {
			ret.topic = kv_parser.GetValue("chanprops_topic");
		}
		if(kv_parser.HasKey("chanprops_entrymsg")) {
			ret.setby = kv_parser.GetValue("chanprops_setby");
		}
		if(kv_parser.HasKey("chanprops_setbypid")) {
			ret.setbypid = kv_parser.GetValueInt("chanprops_setbypid");
		} else {
			ret.setbypid = 0;
		}

		if(kv_parser.HasKey("chanprops_limit")) {
			ret.limit = kv_parser.GetValueInt("chanprops_limit");
		} else {
			ret.limit = 0;
		}

		if(kv_parser.HasKey("chanprops_password")) {
			ret.password = kv_parser.GetValue("chanprops_password");
		}

		return ret;
	}
	void ChatBackendTask::SendClientMessageToDrivers(int target_id, ChatClientInfo user, const char *msg, EChatMessageType message_type) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->OnSendClientMessage(target_id, user, msg, message_type);
			it++;
		}
	}
	void ChatBackendTask::SendChannelMessageToDrivers(ChatChannelInfo channel, ChatClientInfo user, const char *msg, EChatMessageType message_type) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->OnSendChannelMessage(channel, user, msg, message_type);
			it++;
		}
	}
	void ChatBackendTask::SendClientJoinChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendJoinChannelMessage(client, channel);
			it++;
		}
	}
	void ChatBackendTask::SendClientPartChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendPartChannelMessage(client, channel, part_reason, reason_str);
			it++;
		}
	}
	void ChatBackendTask::SendChannelModeUpdateToDrivers(ChatClientInfo client_info, ChatChannelInfo channel_info, ChanModeChangeData change_data) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendChannelModeUpdate(client_info, channel_info, change_data);
			it++;
		}
	}
	void ChatBackendTask::SendUpdateChannelTopicToDrivers(ChatClientInfo client, ChatChannelInfo channel) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendUpdateChannelTopic(client, channel);
			it++;
		}
	}
	void ChatBackendTask::SendSetChannelClientKeysToDrivers(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendSetChannelClientKeys(client, channel, kv_data);
			it++;
		}
	}
	void ChatBackendTask::SendSetChannelKeysToDrivers(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendSetChannelKeys(client, channel, kv_data);
			it++;
		}
	}
	void ChatBackendTask::SendUserQuitMessage(ChatClientInfo client, std::string quit_reason) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendUserQuitMessage(client, quit_reason);
			it++;
		}
	}
	bool ChatBackendTask::TestChannelPermissions(ChatChanClientInfo chan_client_info, ChatChannelInfo channel_info, ChatQueryRequest task_params, struct Chat::_ChatQueryResponse &response) {
		std::ostringstream s;
		ChatClientInfo client_info;
		//

		if(chan_client_info.client_info.operflags & OPERPRIVS_OPEROVERRIDE) {
			return true;
		}
		if(task_params.type == EChatQueryRequestType_UpdateChannelModes) {
			//scan add modes
			for(unsigned int i=0;i<sizeof(ModeFlagPermissions)/sizeof(struct _ModeFlagPermissions);i++) {
				s.str("");
				if(task_params.add_flags & ModeFlagPermissions[i].flag) {
					if(~chan_client_info.client_flags & ModeFlagPermissions[i].required_flag) {
						s << "Set " << ModeFlagPermissions[i].friendly_name << " - " << m_permission_name_map[ModeFlagPermissions[i].required_flag];
						response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_BadPermissions, s.str()));
					}
				}
			}

			//scan remove modes
			for(unsigned int i=0;i<sizeof(ModeFlagPermissions)/sizeof(struct _ModeFlagPermissions);i++) {
				s.str("");
				if(task_params.remove_flags & ModeFlagPermissions[i].flag) {
					if(~chan_client_info.client_flags & ModeFlagPermissions[i].required_flag) {
						s << "Remove " << ModeFlagPermissions[i].friendly_name << " - " << m_permission_name_map[ModeFlagPermissions[i].required_flag];
						response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_BadPermissions, s.str()));
					}
				}
			}

			//scan set user modes
			std::vector<std::pair<std::string, ChanClientModeChange> >::iterator it = task_params.user_modechanges.begin();
			while(it != task_params.user_modechanges.end()) {
				s.str("");
				std::pair<std::string, ChanClientModeChange> p = *it;
				LoadClientInfoByName(client_info, p.first);

				if(!client_info.client_id) {
					s << p.first << " - User not found";
					response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_NoUser_OrChan, s.str()));
				} else {
					ChatChanClientInfo target_chan_client_info = GetChanClientInfo(channel_info.channel_id, task_params.peer->getClientInfo().client_id);

					if(GetUserChanPermissionScore(target_chan_client_info.client_flags) >= GetUserChanPermissionScore(chan_client_info.client_flags)) {
						if(~chan_client_info.client_flags & EChanClientFlags_Owner) {
							s << p.first << " - Cannot modify user modes equal or higher to yours";
							response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_BadPermissions, s.str()));
						}
					}
					if(GetUserChanPermissionScore(p.second.mode_flag) >= GetUserChanPermissionScore(chan_client_info.client_flags)) {
						s << p.first << " - Insufficent Permissions";
						response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_BadPermissions, s.str()));
					}
				}
				it++;
			}
		} else if (task_params.type == EChatQueryRequestType_UpdateChannelTopic) {
			if(GetUserChanPermissionScore(chan_client_info.client_flags) < 2) {
				s << "Change Channel Topic - Half Operator or higher required";
				response.errors.push_back(std::pair<EChatBackendResponseError, std::string>(EChatBackendResponseError_BadPermissions, s.str()));
			}
		}
		return !(response.errors.size() > 0);
	}
	int ChatBackendTask::GetUserChanPermissionScore(int client_flags) {
		if(client_flags & EChanClientFlags_Owner)
			return 4;
		if(client_flags & EChanClientFlags_Op)
			return 3;
		if(client_flags & EChanClientFlags_HalfOp)
			return 2;
		if(client_flags & EChanClientFlags_Voice)
			return 1;

		return 0;
	}
}