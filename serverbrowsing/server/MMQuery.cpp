#include "main.h"

#include "server/SBDriver.h"
#include "MMQuery.h"

#include <OS/socketlib/socketlib.h>

#include <OS/legacy/helpers.h>

#include <OS/Thread.h>

//#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/adapters/libevent.h>
#undef _WINSOCK2API_


namespace MM {
	SB::Driver *mp_driver;
	redisContext *mp_redis_connection;
	redisAsyncContext *mp_redis_async_connection;

	const char *sb_mm_channel = "serverbrowsing.servers";
	void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
	    redisReply *r = (redisReply*)reply;

	    ServerListQuery servers;
	    if (reply == NULL) return;

	    char msg_type[16], server_key[64];

	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		if(strcmp(r->element[1]->str,sb_mm_channel) == 0) {
	    			find_param(0, r->element[2]->str,(char *)&msg_type, sizeof(msg_type));
	    			find_param(1, r->element[2]->str, (char *)&server_key, sizeof(server_key));

		   			if(strcmp(msg_type,"del") == 0) {
	    				AppendServerEntry(server_key, &servers, true, true);
	    				mp_driver->SendDeleteServer(servers);
	    			} else if(strcmp(msg_type,"new") == 0) {
	    				AppendServerEntry(server_key, &servers, true);
	    				mp_driver->SendNewServer(servers);
	    			} else if(strcmp(msg_type,"update") == 0) {
	    				AppendServerEntry(server_key, &servers, true);
	    				mp_driver->SendUpdateServer(servers);
	    			}
	    		}
	    	}
	    }
	}

	void SubmitData(const char *base64, struct sockaddr_in *from, struct sockaddr_in *to, OS::GameData *game) {
		freeReplyObject(redisCommand(mp_redis_connection, "PUBLISH %s \\send_msg\\%s\\%s:%d\\%s:%d\\%s\n",sb_mm_channel,game->gamename,Socket::inet_ntoa(from->sin_addr),from->sin_port,Socket::inet_ntoa(to->sin_addr),to->sin_port,base64));
	}


	void *setup_redis_async(OS::CThread *) {
	    mp_redis_async_connection = redisAsyncConnect("127.0.0.1", 6379);

	    struct event_base *base = event_base_new();
	    redisLibeventAttach(mp_redis_async_connection, base);
	    redisAsyncCommand(mp_redis_async_connection, onRedisMessage, NULL, "SUBSCRIBE %s",sb_mm_channel);
	    event_base_dispatch(base);
		return NULL;
	}
	void Init(SB::Driver *driver) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_driver = driver;
		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		OS::CreateThread(setup_redis_async, NULL, true);


	}
	void AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted) {
		redisReply *reply;

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s deleted", entry_name.c_str());
		if(reply->type != REDIS_REPLY_NIL && !include_deleted) {
			freeReplyObject(reply);
			printf("skip deleted server\n");
			return;	
		}
		freeReplyObject(reply);
		

		Server *server = new MM::Server();

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s gameid", entry_name.c_str());
		if (!reply)
			goto error_cleanup;
		server->game = OS::GetGameByID(atoi(OS::strip_quotes(reply->str).c_str()));
		freeReplyObject(reply);


		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s id", entry_name.c_str());
		if (!reply)
			goto error_cleanup;

		server->id = atoi(OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s wan_port", entry_name.c_str());
		if (!reply)
			goto error_cleanup;
		server->wan_address.port = atoi(reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s wan_ip", entry_name.c_str());
		if (!reply)
			goto error_cleanup;

		server->wan_address.ip = Socket::htonl(Socket::inet_addr(OS::strip_quotes(reply->str).c_str()));
		freeReplyObject(reply);


		if(all_keys) {
			reply = (redisReply *)redisCommand(mp_redis_connection, "HKEYS %scustkeys", entry_name.c_str());
			if (!reply)
				goto error_cleanup;
			if (reply->type == REDIS_REPLY_ARRAY) {
				for (int j = 0; j < reply->elements; j++) {
					std::string search_key = entry_name;
					search_key += "custkeys";
					FindAppend_ServKVFields(server, search_key, reply->element[j]->str);
				}
			}
		} else {
			while (it != ret->requested_fields.end()) {
				std::string field = *it;
				reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %scustkeys %s", entry_name.c_str(), field.c_str());
				if (reply) {
					if (reply->str) {
						server->kvFields[field] = OS::strip_quotes(reply->str);
					}
					freeReplyObject(reply);
				}
				it++;
			}
		}

		ret->list.push_back(server);
		return;

		error_cleanup:
			delete server;

	}
	bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key) {
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
	void AppendGroupEntry(const char *entry_name, ServerListQuery *ret) {
		redisReply *reply;

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s gameid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->game = OS::GetGameByID(atoi(OS::strip_quotes(reply->str).c_str()));
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %s groupid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->wan_address.ip = atoi(OS::strip_quotes(reply->str).c_str());
		freeReplyObject(reply);

		FindAppend_ServKVFields(server, entry_name, "maxwaiting");
		FindAppend_ServKVFields(server, entry_name, "name");
		FindAppend_ServKVFields(server, entry_name, "password");
		FindAppend_ServKVFields(server, entry_name, "numwaiting");
		FindAppend_ServKVFields(server, entry_name, "numservers");


		while (it != ret->requested_fields.end()) {
			std::string field = *it;
			std::string entry = entry_name;
			entry += "custkeys";
			reply = (redisReply *)redisCommand(mp_redis_connection, "HGET %scustkeys %s", entry_name, field.c_str());
			FindAppend_ServKVFields(server, entry, field);
			it++;
		}

		ret->list.push_back(server);
		return;

	error_cleanup:
		delete server;

	}
	ServerListQuery GetServers(const SB::sServerListReq *req) {
		ServerListQuery ret;
		
		ret.requested_fields = req->field_list;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_QR));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";
		
		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendServerEntry(std::string(reply->element[j]->str), &ret, false);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	ServerListQuery GetGroups(const SB::sServerListReq *req) {
		ServerListQuery ret;

		ret.requested_fields = req->field_list;

		freeReplyObject(redisCommand(mp_redis_connection, "SELECT %d", OS::ERedisDB_SBGroups));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";

		redisReply *reply = (redisReply *)redisCommand(mp_redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendGroupEntry(reply->element[j]->str, &ret);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	

}
