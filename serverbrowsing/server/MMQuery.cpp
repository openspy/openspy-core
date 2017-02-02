#include "main.h"

#include "server/SBDriver.h"
#include "MMQuery.h"

#include <OS/socketlib/socketlib.h>

#include <OS/legacy/helpers.h>

#include <OS/Thread.h>

#include <sstream>
#include <algorithm>

#include <serverbrowsing/filter/filter.h>

namespace MM {


	const char *sb_mm_channel = "serverbrowsing.servers";
	MMQueryTask *MMQueryTask::m_task_singleton = NULL;
	void MMQueryTask::onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
		MMQueryTask *task = (MMQueryTask *)privdata;
	    redisReply *r = (redisReply*)reply;

	    Server *server = NULL;
	    if (reply == NULL) return;

	    char msg_type[16], server_key[64];
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		if(strcmp(r->element[1]->str,sb_mm_channel) == 0) {
	    			find_param(0, r->element[2]->str,(char *)&msg_type, sizeof(msg_type));
	    			find_param(1, r->element[2]->str, (char *)&server_key, sizeof(server_key));

	    			server = task->GetServerByKey(server_key, task->mp_redis_async_retrival_connection, strcmp(msg_type,"del") == 0);
	    			if(!server) return;

	    			std::vector<SB::Driver *>::iterator it = task->m_drivers.begin();
	    			while(it != task->m_drivers.end()) {
	    				SB::Driver *driver = *it;
			   			if(strcmp(msg_type,"del") == 0) {
		    				driver->SendDeleteServer(server);
		    			} else if(strcmp(msg_type,"new") == 0) {
		    				driver->SendNewServer(server);
		    			} else if(strcmp(msg_type,"update") == 0) {
		    				driver->SendUpdateServer(server);
		    			}
	    				it++;
	    			}
	    		}
	    	}
	    }
	}

	void *MMQueryTask::setup_redis_async(OS::CThread *thread) {
		MMQueryTask *task = (MMQueryTask *)thread->getParams();

		task->mp_event_base = event_base_new();

	    task->mp_redis_async_connection = redisAsyncConnect("127.0.0.1", 6379);

	    redisLibeventAttach(task->mp_redis_async_connection, task->mp_event_base);
	    redisAsyncCommand(task->mp_redis_async_connection, MMQueryTask::onRedisMessage, thread->getParams(), "SUBSCRIBE %s",sb_mm_channel);
	    event_base_dispatch(task->mp_event_base);
		return NULL;
	}
	void MMQueryTask::AddDriver(SB::Driver *driver) {
		m_drivers.push_back(driver);
	}
	void MMQueryTask::RemoveDriver(SB::Driver *driver) {

	}
	MMQueryTask *MMQueryTask::getQueryTask() {
		if(!MMQueryTask::m_task_singleton) {
			MMQueryTask::m_task_singleton = new MMQueryTask();
		}
		return MMQueryTask::m_task_singleton;
	}
	MMQueryTask::MMQueryTask() {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);
		mp_redis_async_retrival_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		mp_async_thread = OS::CreateThread(setup_redis_async, this, true);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(MMQueryTask::TaskThread, this, true);

	}

	MMQueryTask::~MMQueryTask() {
		delete mp_thread;
		delete mp_async_thread;
		delete mp_mutex;

		redisFree(mp_redis_connection);
		redisFree(mp_redis_async_retrival_connection);
		redisAsyncFree(mp_redis_async_connection);
		event_base_free(mp_event_base);
	}

	void MMQueryTask::Shutdown() {
		MMQueryTask *query_task = getQueryTask();
		

		delete query_task;
	}

	//////////////////////////////////////////////////
	/// Async MM Query code
	void MMQueryTask::AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, redisContext *redis_ctx, const sServerListReq *req) {
		redisReply *reply;
		std::map<std::string, std::string> all_cust_keys; //used for filtering

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/


		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_QR));

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s deleted", entry_name.c_str());
		if(reply) {
			if(reply->type != REDIS_REPLY_NIL && !include_deleted) {
				freeReplyObject(reply);
				return;	
			}
			freeReplyObject(reply);
		} else {
			return;
		}
		

		Server *server = new MM::Server();

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s gameid", entry_name.c_str());

		if (!reply)
			goto error_cleanup;

		if(reply->type == REDIS_REPLY_STRING) {
			std::string gameid_reply = OS::strip_quotes(reply->str).c_str();
			int gameid = atoi(gameid_reply.c_str());
			server->game = OS::GetGameByID(gameid, redis_ctx);
			freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_QR));
		}
		freeReplyObject(reply);

		server->key = entry_name;


		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s id", entry_name.c_str());
		if (!reply)
			goto error_cleanup;

		if(reply->type == REDIS_REPLY_STRING)
			server->id = atoi(OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s wan_port", entry_name.c_str());
		if (!reply)
			goto error_cleanup;

		if(reply->type == REDIS_REPLY_STRING)
			server->wan_address.port = atoi(reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s wan_ip", entry_name.c_str());
		if (!reply)
			goto error_cleanup;
		if(reply->type == REDIS_REPLY_STRING)
			server->wan_address.ip = Socket::htonl(Socket::inet_addr(OS::strip_quotes(reply->str).c_str()));

		freeReplyObject(reply);

		if(all_keys) {
			reply = (redisReply *)redisCommand(redis_ctx, "HKEYS %scustkeys", entry_name.c_str());
			if (!reply)
				goto error_cleanup;
			if (reply->type == REDIS_REPLY_ARRAY) {
				for (int j = 0; j < reply->elements; j++) {
					std::string search_key = entry_name;
					search_key += "custkeys";
					FindAppend_ServKVFields(server, search_key, reply->element[j]->str, redis_ctx);
					if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), reply->element[j]->str) == ret->captured_basic_fields.end()) {
						ret->captured_basic_fields.push_back(reply->element[j]->str);
					}
				}
			}

			int idx = 0;
			uint8_t last_type = REDIS_REPLY_NIL;
			std::string key;
			std::ostringstream s;
			do {
				s << entry_name << "custkeys_player_" << idx;
				key = s.str();
				reply = (redisReply *)redisCommand(redis_ctx, "HKEYS %s", key.c_str());
				if(!reply)
					break;
				last_type = reply->type;
				if (reply->type == REDIS_REPLY_ARRAY) {
					if(reply->elements == 0)  {
						freeReplyObject(reply);
						break;
					}
					for (int j = 0; j < reply->elements; j++) {
						FindAppend_PlayerKVFields(server, key, reply->element[j]->str, idx, redis_ctx);
						if(std::find(ret->captured_player_fields.begin(), ret->captured_player_fields.end(), reply->element[j]->str) == ret->captured_player_fields.end()) {
							ret->captured_player_fields.push_back(reply->element[j]->str);
						}
					}
				}
				s.str("");
				freeReplyObject(reply);
				idx++;
			} while(last_type != REDIS_REPLY_NIL);
			s.str("");
			
			do {
				s << entry_name << "custkeys_team_" << idx;
				key = s.str();
				reply = (redisReply *)redisCommand(redis_ctx, "HKEYS %s", key.c_str());
				if(!reply)
					break;
				last_type = reply->type;
				if (reply->type == REDIS_REPLY_ARRAY) {
					if(reply->elements == 0)  {
						freeReplyObject(reply);
						break;
					}
					for (int j = 0; j < reply->elements; j++) {
						FindAppend_TeamKVFields(server, key, reply->element[j]->str, idx, redis_ctx);
						if(std::find(ret->captured_team_fields.begin(), ret->captured_team_fields.end(), reply->element[j]->str) == ret->captured_team_fields.end()) {
							ret->captured_team_fields.push_back(reply->element[j]->str);
						}
					}
				}
				freeReplyObject(reply);
				s.str("");
				idx++;
			} while(last_type != REDIS_REPLY_NIL);
		} else {

			reply = (redisReply *)redisCommand(redis_ctx, "HKEYS %scustkeys", entry_name.c_str());
			if (!reply)
				goto error_cleanup;
			if (reply->type == REDIS_REPLY_ARRAY) {
				for (int j = 0; j < reply->elements; j++) {
					std::string search_key = entry_name;
					search_key += "custkeys";
					FindAppend_ServKVFields(server, search_key, reply->element[j]->str, redis_ctx);
				}
				all_cust_keys = server->kvFields;
				server->kvFields.clear();
				std::map<std::string, std::string>::iterator it = all_cust_keys.begin();
				while(it != all_cust_keys.end()) {
					std::pair<std::string, std::string> p = *it;
					if(std::find(ret->requested_fields.begin(), ret->requested_fields.end(), p.first) != ret->requested_fields.end()) {
						server->kvFields[p.first] = p.second;
					}
					it++;
				}
			}
			/*while (it != ret->requested_fields.end()) {
				std::string field = *it;
				reply = (redisReply *)redisCommand(redis_ctx, "HGET %scustkeys %s", entry_name.c_str(), field.c_str());
				if (reply) {
					if (reply->str) {
						server->kvFields[field] = OS::strip_quotes(reply->str);
					}
					freeReplyObject(reply);
				}
				it++;
			}*/

		}

		if(!req || filterMatches(req->filter.c_str(), all_cust_keys)) {
			ret->list.push_back(server);
		}
		return;

		error_cleanup:
			delete server;

	}
	bool MMQueryTask::FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx)
	 {
		redisReply *reply = (redisReply *)redisCommand(redis_ctx, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (!reply)
			return false;
		if (reply->type == REDIS_REPLY_STRING) {
			server->kvPlayers[index][key] = OS::strip_quotes(reply->str);
		}
		else if(reply->type == REDIS_REPLY_INTEGER) {
			server->kvPlayers[index][key] = reply->integer;
		}
		freeReplyObject(reply);

		return true;

	}
	bool MMQueryTask::FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx) {
		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (!reply)
			return false;
		if (reply->type == REDIS_REPLY_STRING) {
			server->kvTeams[index][key] = OS::strip_quotes(reply->str);
		}
		else if(reply->type == REDIS_REPLY_INTEGER) {
			server->kvTeams[index][key] = reply->integer;
		}
		freeReplyObject(reply);

		return true;

	}
	bool MMQueryTask::FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key, redisContext *redis_ctx) {
		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (!reply)
			return false;
		if (reply->type == REDIS_REPLY_STRING) {
			server->kvFields[key] = OS::strip_quotes(reply->str);
		}
		else if(reply->type == REDIS_REPLY_INTEGER) {
			server->kvFields[key] = reply->integer;
		}
		freeReplyObject(reply);

		return true;

	}
	void MMQueryTask::AppendGroupEntry(const char *entry_name, ServerListQuery *ret, redisContext *redis_ctx, bool all_keys) {
		redisReply *reply;

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_SBGroups));

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s gameid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->game = OS::GetGameByID(atoi(OS::strip_quotes(reply->str).c_str()));
		freeReplyObject(reply);
		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_SBGroups)); //change context back to SB db id

		reply = (redisReply *)redisCommand(redis_ctx, "HGET %s groupid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->wan_address.ip = atoi(OS::strip_quotes(reply->str).c_str()); //for V2
		server->kvFields["groupid"] = OS::strip_quotes(reply->str).c_str(); //for V1

		freeReplyObject(reply);

		FindAppend_ServKVFields(server, entry_name, "maxwaiting", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "hostname", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "password", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "numwaiting", redis_ctx);
		FindAppend_ServKVFields(server, entry_name, "numservers", redis_ctx);

		if(all_keys) {
			reply = (redisReply *)redisCommand(redis_ctx, "HKEYS %scustkeys", entry_name);
			if (!reply)
				goto error_cleanup;
			if (reply->type == REDIS_REPLY_ARRAY) {
				for (int j = 0; j < reply->elements; j++) {
					std::string search_key = entry_name;
					search_key += "custkeys";
					FindAppend_ServKVFields(server, search_key, reply->element[j]->str, redis_ctx);
					if(std::find(ret->captured_basic_fields.begin(), ret->captured_basic_fields.end(), reply->element[j]->str) == ret->captured_basic_fields.end()) {
						ret->captured_basic_fields.push_back(reply->element[j]->str);
					}
				}
			}
		} else {
			while (it != ret->requested_fields.end()) {
				std::string field = *it;
				std::string entry = entry_name;
				entry += "custkeys";
				reply = (redisReply *)redisCommand(redis_ctx, "HGET %scustkeys %s", entry_name, field.c_str());
				FindAppend_ServKVFields(server, entry, field, redis_ctx);
				it++;
			}
	}

		ret->list.push_back(server);
		return;

	error_cleanup:
		delete server;

	}
	ServerListQuery MMQueryTask::GetServers(const sServerListReq *req) {
		ServerListQuery ret;
		
		ret.requested_fields = req->field_list;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_QR));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";
		
		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendServerEntry(std::string(reply->element[j]->str), &ret, req->all_keys, false, mp_redis_connection, req);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	ServerListQuery MMQueryTask::GetGroups(const sServerListReq *req) {
		ServerListQuery ret;

		ret.requested_fields = req->field_list;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_SBGroups));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendGroupEntry(reply->element[j]->str, &ret, mp_redis_connection, req->all_keys);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	Server *MMQueryTask::GetServerByKey(std::string key, redisContext *redis_ctx, bool include_deleted) {
		if(redis_ctx == NULL) {
			redis_ctx = mp_redis_connection;
		}
		ServerListQuery ret;
		AppendServerEntry(key, &ret, true, include_deleted, redis_ctx, NULL);
		if(ret.list.size() < 1)
			return NULL;

		return ret.list[0];
	}
	Server *MMQueryTask::GetServerByIP(OS::Address address, OS::GameData game, redisContext *redis_ctx) {
		Server *server = NULL;
		if(redis_ctx == NULL) {
			redis_ctx = mp_redis_connection;
		}
		std::ostringstream s;
		
		struct sockaddr_in addr;
		addr.sin_port = Socket::htons(address.port);
		addr.sin_addr.s_addr = Socket::htonl(address.ip);
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);

		freeReplyObject(redisCommand(redis_ctx, "SELECT %d", OS::ERedisDB_QR));

		s << "GET IPMAP_" << game.gamename << "_" << ipinput << "-" << address.port;
		std::string cmd = s.str();
		redisReply *reply = (redisReply *)redisCommand(redis_ctx, cmd.c_str());
		if(reply->type == REDIS_REPLY_STRING) {
			server = GetServerByKey(reply->str, redis_ctx);
		}

		freeReplyObject(reply);
		return server;
	}
	void MMQueryTask::FreeServerListQuery(struct MM::ServerListQuery *query) {
		std::vector<Server *>::iterator it = query->list.begin();
		while(it != query->list.end()) {
			Server *server = *it;
			delete server;
			it++;
		}
	}

	void MMQueryTask::PerformServersQuery(MMQueryRequest request) {
		request.callback(request, GetServers(&request.req), request.extra);
	}
	void MMQueryTask::PerformGroupsQuery(MMQueryRequest request) {
		request.callback(request, GetGroups(&request.req), request.extra);
	}
	void MMQueryTask::PerformGetServerByKey(MMQueryRequest request) {
		ServerListQuery ret;
		AppendServerEntry(request.key, &ret, true, false, NULL, NULL);
		request.callback(request, ret, request.extra);
		FreeServerListQuery(&ret);
	}
	void MMQueryTask::PerformGetServerByIP(MMQueryRequest request) {
		ServerListQuery ret;
		Server *serv = GetServerByIP(request.address, request.SubmitData.game, NULL);
		if(serv)
			ret.list.push_back(serv);
		request.callback(request, ret, request.extra);
		FreeServerListQuery(&ret);
	}
	void MMQueryTask::PerformSubmitData(MMQueryRequest request) {
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\send_msg\\%s\\%s\\%d\\%s\\%d\\%s\n",
			sb_mm_channel,request.SubmitData.game.gamename,Socket::inet_ntoa(request.SubmitData.from.sin_addr),
			request.SubmitData.from.sin_port,Socket::inet_ntoa(request.SubmitData.to.sin_addr),
			request.SubmitData.to.sin_port,request.SubmitData.base64.c_str()));
	}
	void *MMQueryTask::TaskThread(OS::CThread *thread) {
		MMQueryTask *task = (MMQueryTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<MMQueryRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					MMQueryRequest task_params = *it;
					switch(task_params.type) {
						case EMMQueryRequestType_GetServers:
							task->PerformServersQuery(task_params);
							break;
						case EMMQueryRequestType_GetGroups:
							task->PerformGroupsQuery(task_params);
							break;
						case EMMQueryRequestType_GetServerByKey:
							task->PerformGetServerByKey(task_params);
							break;
						case EMMQueryRequestType_GetServerByIP:
						task->PerformGetServerByIP(task_params);
							break;
						case EMMQueryRequestType_SubmitData:
							task->PerformSubmitData(task_params);
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
}
