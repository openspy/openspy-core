#include "ChatBackend.h"
#include "ChatPeer.h"
#include <OS/legacy/helpers.h>
#include <sstream>
namespace Chat {
	const char *mp_pk_name = "CLIENTID";
	const char *mp_chan_pk_name = "CHANID";
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
		req.query_name = name;
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
		resp.error = EChatBackendResponseError_NoError;

		if(resp.channel_info.channel_id == 0 && !no_create) {
			resp.channel_info = CreateChannel(task_params.query_name);
		}

		if(resp.channel_info.channel_id == 0) {
			resp.error = EChatBackendResponseError_NoUser_OrChan;
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
			client_id = atoi(reply->str);
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			client_id = reply->integer;
		}

		if(reply->type != REDIS_REPLY_NIL) {
			LoadClientInfoByID(info, client_id);
		}
		struct Chat::_ChatQueryResponse response;
		response.client_info = info;

		if(client_id == 0) {
			response.error = EChatBackendResponseError_NoUser_OrChan;
		} else {
			response.error = EChatBackendResponseError_NoError;
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
	void ChatBackendTask::SubmitRemoveUserFromChannel(ChatQueryCB cb, Peer *peer, void *extra, ChatChannelInfo channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_RemoveUserFromChannel;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
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
	void ChatBackendTask::SubmitUpdateChannelModes(ChatQueryCB cb, Peer *peer, void *extra, uint32_t addmask, uint32_t removemask, ChatChannelInfo channel) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_UpdateChannelModes;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.query_data.client_info = peer->getClientInfo();
		req.query_data.channel_info = channel;
		req.add_flags = addmask;
		req.remove_flags = removemask;
		getQueryTask()->AddRequest(req);
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
			response.error = EChatBackendResponseError_NoUser_OrChan;
			goto end_error;
		}
		
		response.error = EChatBackendResponseError_NoError;
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
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));
		if(task_params.query_data.channel_info.channel_id != 0) {
			channel_info = GetChannelByID(task_params.query_data.channel_info.channel_id);
		} else {
			channel_info = GetChannelByName(task_params.query_data.channel_info.name);
		}

		if(channel_info.channel_id == 0) {
			response.error = EChatBackendResponseError_NoUser_OrChan;
			goto end_error;
		}
		//test source permissions
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chan_%d_client_%d client_flags", channel_info.channel_id, task_params.query_data.client_info.client_id);
		
		if(reply && reply->type == REDIS_REPLY_INTEGER) {
			client_chan_flags = reply->integer;
		} else if(reply && reply->type == REDIS_REPLY_STRING) {
			client_chan_flags = atoi(reply->str);
		}
		freeReplyObject(reply);
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

		if(modeflags == old_modeflags) {
			response.error = EChatBackendResponseError_NoChange;
			goto end_error;
		}

		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d modeflags %d", channel_info.channel_id, modeflags));


		channel_info.modeflags = modeflags;
		response.channel_info = channel_info;

		chan_str = ChannelInfoToKVString(channel_info);
		user_str = ClientInfoToKVString(task_params.peer->getClientInfo());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\channel_update_modeflags\\%s%s\\old_modeflags\\%d\n",
			chat_messaging_channel, chan_str.c_str(), user_str.c_str(), old_modeflags));

		response.error = EChatBackendResponseError_NoError;

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
		redisReply *reply;
		ChatChannelInfo channel_info;
		ChatClientInfo client_info;
		std::string chan_str;
		std::string user_str;
		struct Chat::_ChatQueryResponse response;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		if(task_params.query_data.channel_info.channel_id != 0) {
			channel_info = GetChannelByID(task_params.query_data.channel_info.channel_id);
		} else {
			channel_info = GetChannelByName(task_params.query_data.channel_info.name);
		}

		client_info = task_params.peer->getClientInfo();

		if(channel_info.channel_id == 0) {
			response.error = EChatBackendResponseError_NoUser_OrChan;
			goto end_error;
		}
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic \"%s\"", channel_info.channel_id, task_params.message.c_str()));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic_seton %d", channel_info.channel_id, time(NULL)));
		freeReplyObject(redisCommand(mp_redis_connection, "HSET chat_channel_%d topic_setby %s", channel_info.channel_id, client_info.name.c_str()));

		response.error = EChatBackendResponseError_NoError;

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

		freeReplyObject(redisCommand(mp_redis_connection, "HPUSH chan_%d_client_%d client_flags 0", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));
		freeReplyObject(redisCommand(mp_redis_connection, "HPUSH chan_%d_client_%d_keys testkey \"dbg_test_key\"", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));

		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\user_join_channel%s%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str()));

	}
	void ChatBackendTask::PerformSendRemoveUserFromChannel(ChatQueryRequest task_params) {
		std::string chan_str = ChannelInfoToKVString(task_params.query_data.channel_info);
		std::string user_str = ClientInfoToKVString(task_params.peer->getClientInfo());

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		freeReplyObject(redisCommand(mp_redis_connection, "LREM chan_%d_clients 0 %d", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\user_leave_channel%s%s\n",
			chat_messaging_channel, chan_str.c_str(),user_str.c_str()));
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

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d nick \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.name.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d user \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.user.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d realname \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.realname.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d host \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.hostname.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d ip \"%s\"", task_params.query_data.client_info.client_id,task_params.query_data.client_info.ip.ToString().c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET nick_id_map %s %d", task_params.query_data.client_info.name.c_str(), task_params.query_data.client_info.client_id);
		freeReplyObject(reply);

		struct Chat::_ChatQueryResponse response;
		response.client_info = task_params.query_data.client_info;
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
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
		for(int i=0;i<reply->elements;i++) {
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

		/*
		freeReplyObject(redisCommand(mp_redis_connection, "HPUSH chan_%d_client_%d_keys testkey \"dbg_test_key\"", task_params.query_data.channel_info.channel_id, task_params.query_data.client_info.client_id));
		*/

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
		for(int i=0;i<reply->elements;i++) {
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

		uint8_t *out;
		int len;
	    int type;
	    char msg_type[32];
	    char msg[CHAT_MAX_MESSAGE + 1];
	    std::map<std::string,std::string> key_data;
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		if(strcmp(r->element[1]->str,chat_messaging_channel) == 0) {
	    			type = find_paramint("msg_type", r->element[2]->str);
	    			if(!find_param("type", r->element[2]->str,(char *)&msg_type, sizeof(msg_type))) {
	    					return;
	    			}
					if(strcmp(msg_type, "send_client_msg") == 0) {
		    			if(!find_param("msg", r->element[2]->str,(char *)&msg, CHAT_MAX_MESSAGE)) {
		    					return;
		    			}
		    			type = find_paramint("msg_type", r->element[2]->str);
		    			int target_id = find_paramint("to_id", r->element[2]->str);
						ChatClientInfo user = ClientInfoFromKVString(r->element[2]->str);
						OS::Base64StrToBin(msg, &out, len);
						task->SendClientMessageToDrivers(target_id,user, (const char *)out, (EChatMessageType)type);
						free((void *)out);
					} else if(strcmp(msg_type, "send_channel_msg") == 0) {
		    			if(!find_param("msg", r->element[2]->str,(char *)&msg, CHAT_MAX_MESSAGE)) {
		    					return;
		    			}
		    			type = find_paramint("msg_type", r->element[2]->str);
						ChatClientInfo user = ClientInfoFromKVString(r->element[2]->str);
						ChatChannelInfo channel = ChannelInfoFromKVString(r->element[2]->str);
						OS::Base64StrToBin(msg, &out, len);
						task->SendChannelMessageToDrivers(channel,user, (const char *)out, (EChatMessageType)type);
						free((void *)out);
					} else if(strcmp(msg_type, "user_join_channel") == 0) {
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						task->SendClientJoinChannelToDrivers(client_info, channel_info);
					} else if(strcmp(msg_type, "user_leave_channel") == 0) {
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						task->SendClientPartChannelToDrivers(client_info, channel_info);
					} else if(strcmp(msg_type, "channel_update_modeflags") == 0) {
						ChanModeChangeData change_data;
						change_data.old_modeflags = find_paramint("old_modeflags", r->element[2]->str);
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						task->SendChannelModeUpdateToDrivers(client_info, channel_info, change_data);
					} else if(strcmp(msg_type, "channel_update_topic") == 0) {
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);
						task->SendUpdateChannelTopicToDrivers(client_info, channel_info);
					} else if(strcmp(msg_type, "set_channel_client_keys") == 0) {
		    			if(!find_param("key_data", r->element[2]->str,(char *)&msg, CHAT_MAX_MESSAGE)) {
		    					return;
		    			}
						
						client_info = ClientInfoFromKVString(r->element[2]->str);
						channel_info = ChannelInfoFromKVString(r->element[2]->str);

						OS::Base64StrToBin(msg, &out, len);

						key_data = OS::KeyStringToMap(std::string((const char *)out));

						task->SendSetChannelClientKeysToDrivers(client_info, channel_info, key_data);

						free((void *)out);
					}
	    		}
	    	}
	    }
	}
	std::string ChatBackendTask::ClientInfoToKVString(ChatClientInfo info) {
		std::ostringstream s;
		s << "\\client_nick\\" << info.name;
		s << "\\client_user\\" << info.user;
		s << "\\client_realname\\" << info.realname;
		s << "\\client_host\\" << info.hostname;
		s << "\\client_ip\\" << info.ip.ToString();
		s << "\\client_id\\" << info.client_id;
		return s.str();
	}
	ChatClientInfo ChatBackendTask::ClientInfoFromKVString(const char *str) {
		ChatClientInfo ret;
		char data[CHAT_MAX_MESSAGE + 1];
		if(find_param("client_nick", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.name = data;
		}

		if(find_param("client_user", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.user = data;
		}
		if(find_param("client_realname", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.realname = data;
		}
		if(find_param("client_host", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.hostname = data;
		}
		if(find_param("client_ip", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.ip = OS::Address(data);
		}
		if(find_param("client_id", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.client_id = atoi(data);
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
		return s.str();
	}
	ChatChannelInfo ChatBackendTask::ChannelInfoFromKVString(const char *str) {
		ChatChannelInfo ret;
		char data[CHAT_MAX_MESSAGE + 1];
		if(find_param("channel_name", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.name = data;
		}
		if(find_param("channel_topic_message", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			uint8_t *out;
			int len;
			OS::Base64StrToBin(data, &out, len);
			ret.topic = (const char *)out;
			free((void *)out);
		}
		if(find_param("channel_topic_setby", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.topic_setby = data;
		}
		if(find_param("channel_topic_seton", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.topic_seton = atoi(data);
		}
		if(find_param("channel_id", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.channel_id = atoi(data);
		}
		if(find_param("channel_modeflags", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.modeflags = atoi(data);
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
	void ChatBackendTask::SendClientPartChannelToDrivers(ChatClientInfo client, ChatChannelInfo channel) {
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->SendPartChannelMessage(client, channel);
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
}