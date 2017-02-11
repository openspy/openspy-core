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
	void ChatBackendTask::SubmitGetChanneltInfoByName(std::string name, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_GetChannelByName;
		req.query_name = name;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		getQueryTask()->AddRequest(req);
	}
	void ChatBackendTask::SubmitClientMessage(int target_id, std::string message, ChatQueryCB cb, Peer *peer, void *extra) {
		ChatQueryRequest req;
		req.type = EChatQueryRequestType_SendClientMessage;
		req.query_data.client_info.client_id = target_id;
		req.callback = cb;
		req.peer = peer;
		req.extra = extra;
		req.message = message;
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
	void ChatBackendTask::PerformFind_OrCreateChannel(ChatQueryRequest task_params) {
		struct Chat::_ChatQueryResponse resp;
		resp.channel_info =  GetChannelByName(task_params.query_name);
		if(resp.channel_info.channel_id == 0) {
			resp.channel_info = CreateChannel(task_params.query_name);
		}

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
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
		freeReplyObject(reply);
	}
	void ChatBackendTask::LoadClientInfoByID(ChatClientInfo &info, int client_id) {
		info.client_id = client_id;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d nick", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.name = reply->str;
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d user", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.user = reply->str;
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d realname", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.realname = reply->str;
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d host", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.hostname = reply->str;
		}
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_client_%d ip", client_id);
		if(reply->type == REDIS_REPLY_STRING) {
			info.ip = OS::Address(reply->str);
		}
		freeReplyObject(reply);

	}
	void ChatBackendTask::PerformSendClientMessage(ChatQueryRequest task_params) {
		std::string kv_user = ClientInfoToKVString(task_params.peer->getClientInfo());
		const char *b64_msg = OS::BinToBase64Str((const uint8_t*)task_params.message.c_str(),task_params.message.length());
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\type\\send_client_msg\\%s\\to_id\\%d\\msg\\%s\n",
			chat_messaging_channel, kv_user.c_str(), task_params.query_data.client_info.client_id, b64_msg));

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

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d nick %s", task_params.query_data.client_info.client_id,task_params.query_data.client_info.name.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d user %s", task_params.query_data.client_info.client_id,task_params.query_data.client_info.user.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d realname %s", task_params.query_data.client_info.client_id,task_params.query_data.client_info.realname.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d host %s", task_params.query_data.client_info.client_id,task_params.query_data.client_info.hostname.c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET chat_client_%d ip %s", task_params.query_data.client_info.client_id,task_params.query_data.client_info.ip.ToString().c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HSET nick_id_map %s %d", task_params.query_data.client_info.name.c_str(), task_params.query_data.client_info.client_id);
		freeReplyObject(reply);

		struct Chat::_ChatQueryResponse response;
		response.client_info = task_params.query_data.client_info;
		task_params.callback(task_params, response, task_params.peer, task_params.extra);
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
		ret.channel_id = 0;
		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat));

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d topic", id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.topic = reply->str;
		}
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET chat_channel_%d name", id);
		if(reply->type == REDIS_REPLY_STRING) {
			ret.name = reply->str;
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
					switch(task_params.type) {
						case EChatQueryRequestType_GetClientByName:
							task->PerformGetClientInfoByName(task_params);
						break;
						case EChatQueryRequestType_GetChannelByName:
							printf("Get chan by name\n");
						break;
						case EChatQueryRequestType_UpdateOrInsertClient:
							task->PerformUpdateOrInsertClient(task_params);
						break;
						case EChatQueryRequestType_SendClientMessage:
							task->PerformSendClientMessage(task_params);
						break;
						case EChatQueryRequestType_Find_OrCreate_Channel:
							task->PerformFind_OrCreateChannel(task_params);
						break;
					}
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(TASK_SLEEP_TIME);
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
	    printf("onRedisMessage\n");
	    int to_profileid, from_profileid;
	    char msg_type[16];
	    char msg[CHAT_MAX_MESSAGE + 1];
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		if(strcmp(r->element[1]->str,chat_messaging_channel) == 0) {
	    			if(!find_param("type", r->element[2]->str,(char *)&msg_type, sizeof(msg_type))) {
	    					return;
	    			}
					printf("Got msg: %s\n", msg_type);
					if(strcmp(msg_type, "send_client_msg") == 0) {
		    			if(!find_param("msg", r->element[2]->str,(char *)&msg, CHAT_MAX_MESSAGE)) {
		    					return;
		    			}
		    			int target_id = find_paramint("to_id", r->element[2]->str);
						ChatClientInfo user = ClientInfoFromKVString(r->element[2]->str);
						uint8_t *out;
						int len;
						OS::Base64StrToBin(msg, &out, len);
						task->SendClientMessageToDrivers(target_id,user, (const char *)out);
						free((void *)out);
					}
	    		}
	    	}
	    }
	}
	std::string ChatBackendTask::ClientInfoToKVString(ChatClientInfo info) {
		std::ostringstream s;
		s << "\\nick\\" << info.name << "\\user\\" << info.user << "\\realname\\" << info.realname << "\\host\\" << info.hostname << "\\ip\\" << info.ip.ToString() << "\\client_id\\" << info.client_id;
		return s.str();
	}
	ChatClientInfo ChatBackendTask::ClientInfoFromKVString(const char *str) {
		ChatClientInfo ret;
		char data[CHAT_MAX_MESSAGE + 1];
		if(find_param("nick", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.name = data;
		}

		if(find_param("user", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.user = data;
		}
		if(find_param("realname", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.realname = data;
		}
		if(find_param("host", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.hostname = data;
		}
		if(find_param("ip", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.ip = OS::Address(data);
		}
		if(find_param("client_id", (char *)str, (char *)&data, CHAT_MAX_MESSAGE)) {
			ret.client_id = atoi(data);
		}
		return ret;
	}
	void ChatBackendTask::SendClientMessageToDrivers(int target_id, ChatClientInfo user, const char *msg) {
		printf("SendClientMessageToDrivers: %d - %s\n", target_id, msg);
		std::vector<Chat::Driver *>::iterator it = m_drivers.begin();
		while(it != m_drivers.end()) {
			Chat::Driver *driver = *it;
			driver->OnSendClientMessage(target_id, user, msg);
			it++;
		}
	}
}