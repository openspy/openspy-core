#include "main.h"
#include <hiredis/hiredis.h>
#include "server/SBDriver.h"
#include "MMQuery.h"

#include <OS/socketlib/socketlib.h>
namespace MM {
	
	void Init() {

	}
	void AppendServerEntry(const char *entry_name, ServerListQuery *ret) {
		redisReply *reply;

		/*
		XXX: add redis error checks, cleanup on error, etc
		*/

		std::vector<std::string>::iterator it = ret->requested_fields.begin();
		Server *server = new MM::Server();

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s gameid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->game = OS::GetGameByID(atoi(reply->str));
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s wan_port", entry_name);
		if (!reply)
			goto error_cleanup;
		server->wan_address.port = atoi(reply->str);
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s wan_ip", entry_name);
		if (!reply)
			goto error_cleanup;
		server->wan_address.ip = Socket::htonl(inet_addr(reply->str));
		freeReplyObject(reply);


		while (it != ret->requested_fields.end()) {
			std::string field = *it;
			reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %scustkeys %s", entry_name, field.c_str());
			if (reply) {
				if (reply->str) {
					server->kvFields[field] = reply->str;
				}
				freeReplyObject(reply);
			}
			it++;
		}

		ret->list.push_back(server);
		return;

	error_cleanup:
		delete server;

	}
	bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key) {
		redisReply *reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (!reply)
			return false;
		if (reply->type == REDIS_REPLY_STRING) {
			server->kvFields[key] = reply->str;
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

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s gameid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->game = OS::GetGameByID(atoi(reply->str));
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %s groupid", entry_name);
		if (!reply)
			goto error_cleanup;
		server->wan_address.ip = atoi(reply->str);
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
			reply = (redisReply *)redisCommand(OS::redis_connection, "HGET %scustkeys %s", entry_name, field.c_str());
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

		freeReplyObject(redisCommand(OS::redis_connection, "SELECT %d", OS::ERedisDB_QR));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";
		
		redisReply *reply = (redisReply *)redisCommand(OS::redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendServerEntry(reply->element[j]->str, &ret);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	ServerListQuery GetGroups(const SB::sServerListReq *req) {
		ServerListQuery ret;

		ret.requested_fields = req->field_list;

		freeReplyObject(redisCommand(OS::redis_connection, "SELECT %d", OS::ERedisDB_SBGroups));
		std::string cmd = "KEYS " + std::string(req->m_for_game.gamename) + ":*:";

		redisReply *reply = (redisReply *)redisCommand(OS::redis_connection, cmd.c_str());
		if (reply->type == REDIS_REPLY_ARRAY) {
			for (int j = 0; j < reply->elements; j++) {
				AppendGroupEntry(reply->element[j]->str, &ret);
			}
		}
		freeReplyObject(reply);
		return ret;
	}
	

}
