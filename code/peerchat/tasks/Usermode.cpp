#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	const char *mp_pk_temporary_usermode_id = "USERMODEID";
	int GetTemporaryUsermodeId(TaskThreadData *thread_data) {
		int ret = -1;

        redisReply *reply;

		redisAppendCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
		redisAppendCommand(thread_data->mp_redis_connection, "INCR %s", mp_pk_temporary_usermode_id);
		
		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
		freeReplyObject(reply);

		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);	
		ret = reply->integer;
		freeReplyObject(reply);
		
		return -ret;
	}
	UsermodeRecord GetUsermodeFromJson(json_t* item) {
		UsermodeRecord record;

		json_t* subitem;

		record.isGlobal = true;

		subitem = json_object_get(item, "id");
		if (subitem != NULL) {
			record.usermodeid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "channelmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.chanmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "hostmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.hostmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "comment");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.comment = json_string_value(subitem);
		}

		subitem = json_object_get(item, "machineid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.machineid = json_string_value(subitem);
		}

		subitem = json_object_get(item, "profileid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "modeflags");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.modeflags = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "gameid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.gameid = json_integer_value(subitem);
			record.has_gameid = true;
		}

		subitem = json_object_get(item, "expiresAt");
		record.expires_at.tv_sec = 0;
		record.expires_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.expires_at.tv_sec = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "setByNick");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.nick = json_string_value(subitem);
		}

		subitem = json_object_get(item, "setByHost");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.hostname = json_string_value(subitem);
		}

		subitem = json_object_get(item, "setByPid");
		record.setByUserSummary.profileid = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "setAt");
		record.set_at.tv_sec = 0;
		record.set_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.set_at.tv_sec = json_integer_value(subitem);
		}
		
		return record;
	}
	void LoadUsermodeFromCache(TaskThreadData* thread_data, std::string cacheKey, UsermodeRecord &record) {
		redisReply *reply;
		const char *args[] = {
			"HMGET", cacheKey.c_str(), 
			"chanmask",
			"hostname",
			"comment",
			"machineid"	,
			"profileid",
			"modeflags",
			"gameid",
			"expiresAt",
			"setAt",
			"setBy",
			"setByHost",
			"setByPid",
			"setByNick"
		};
		reply = (redisReply *)redisCommandArgv(thread_data->mp_redis_connection, sizeof(args) / sizeof(const char *), args, NULL);

		record.usermodeid = atoi(reply->element[0]->str);

		if(reply->element[1]->type == REDIS_REPLY_STRING) {
			record.chanmask = reply->element[1]->str;
		}
		if(reply->element[2]->type == REDIS_REPLY_STRING) {
			record.hostmask = reply->element[2]->str;
		}
		if(reply->element[3]->type == REDIS_REPLY_STRING) {
			record.comment = reply->element[3]->str;
		}
		if(reply->element[4]->type == REDIS_REPLY_STRING) {
			record.machineid = reply->element[4]->str;
		}
		if(reply->element[5]->type == REDIS_REPLY_STRING) {
			record.profileid = atoi(reply->element[5]->str);
		}
		if(reply->element[6]->type == REDIS_REPLY_STRING) {
			record.modeflags = atoi(reply->element[6]->str);
		}

		record.has_gameid = false;
		if(reply->element[7]->type == REDIS_REPLY_STRING) {
			record.gameid = atoi(reply->element[7]->str);
			record.has_gameid = true;
		}
		if(reply->element[8]->type == REDIS_REPLY_STRING) {
			record.expires_at.tv_sec = atoi(reply->element[8]->str);
		}
		if(reply->element[9]->type == REDIS_REPLY_STRING) {
			record.set_at.tv_sec = atoi(reply->element[9]->str);
		}

		if(reply->element[10]->type == REDIS_REPLY_STRING) {
			record.setByUserSummary = UserSummary(reply->element[10]->str);
		} else {
			if(reply->element[11]->type == REDIS_REPLY_STRING) {
				record.setByUserSummary.hostname = reply->element[11]->str;
			}
			if(reply->element[12]->type == REDIS_REPLY_STRING) {
				record.setByUserSummary.profileid = atoi(reply->element[12]->str);
			}
			if(reply->element[13]->type == REDIS_REPLY_STRING) {
				record.setByUserSummary.nick = reply->element[13]->str;
			}
		}
		freeReplyObject(reply);
	}
	bool UsermodeMatchesUser(UsermodeRecord usermode, UserSummary summary) {
		if(usermode.profileid != 0 && summary.profileid == usermode.profileid) {
			return true;
		}

		if(usermode.hostmask.length() > 0 && (stricmp(usermode.hostmask.c_str(), summary.hostname.c_str()) == 0 || match(usermode.hostmask.c_str(), summary.hostname.c_str()) == 0)) {
			return true;
		}

		if(usermode.machineid.length() > 0 && (stricmp(usermode.machineid.c_str(), summary.realname.c_str()) == 0 || match(usermode.machineid.c_str(), summary.realname.c_str()) == 0)) {
			return true;
		}

		if(usermode.has_gameid && usermode.gameid == summary.gameid) {
			return true;
		}
		return false;
	}
	int getEffectiveUsermode(TaskThreadData* thread_data, int channel_id, UserSummary summary, Peer *peer) {

		int modeflags = 0;
		UsermodeRecord record;

		std::string keyname;

		redisReply *reply;

		int cursor = 0;
		//scan channel usermodes
		do {
			reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "HSCAN channel_%d_usermodes %d match *", channel_id, cursor);

            if (reply == NULL || thread_data->mp_redis_connection->err || reply->elements < 2) {
				if(reply) {
					freeReplyObject(reply);
				}
                goto error_cleanup;
            }

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			for(size_t i=0;i<reply->element[1]->elements;i++) {
				if(i % 2 == 0) {
					keyname = std::string("USERMODE_") + reply->element[1]->element[i]->str;
					record = UsermodeRecord();
					LoadUsermodeFromCache(thread_data, keyname, record);
					if(UsermodeMatchesUser(record, summary)) {
						modeflags |= record.modeflags;
					}
					
				}	
			}
			freeReplyObject(reply);
		} while(cursor != 0);


		/*
			No permitted gameid match found, therefore user is banned
		*/
		/*if((modeflags & EUserChannelFlag_GameidPermitted) == 0) {
			modeflags |= EUserChannelFlag_Banned;
		}*/

		error_cleanup:
		return modeflags;
	}

	void WriteUsermodeToCache(UsermodeRecord usermode, TaskThreadData* thread_data) { 
		std::ostringstream ss;
		ss << "USERMODE_" << usermode.usermodeid;
		std::string key_name = ss.str();
		ss.str("");
		

		ss << usermode.usermodeid;
		std::string id_str = ss.str();
		ss.str("");

		ss << usermode.profileid;
		std::string pid_str = ss.str();
		ss.str("");

		ss << usermode.modeflags;
		std::string modeflags_str = ss.str();
		ss.str("");

		ss << usermode.setByUserSummary.profileid;
		std::string setbypid_str = ss.str();
		ss.str("");

		ss << usermode.expires_at.tv_sec;
		std::string expiresAt_str = ss.str();
		ss.str("");

		ss << usermode.gameid;
		std::string gameid_str = ss.str();
		ss.str("");

		const char *args[] = {
			"HMSET", key_name.c_str(), 
			"id", id_str.c_str(),
			"chanmask", usermode.chanmask.c_str(),
			"hostmask", usermode.hostmask.c_str(),
			"comment", usermode.comment.c_str(),
			"machineid", usermode.machineid.c_str(),
			"profileid", pid_str.c_str(),
			"modeflags", modeflags_str.c_str(),
			"isGlobal",  (usermode.usermodeid > 0) ? "1" : "0",
			"setByNick", usermode.setByUserSummary.nick.c_str(),
			"setByHost", usermode.setByUserSummary.hostname.c_str(),
			"setByPid", setbypid_str.c_str(),
			"expiresAt", expiresAt_str.c_str(),
			"gameid", gameid_str.c_str()
			
		};

		int num_args = sizeof(args) / sizeof(const char *);

		if(!usermode.has_gameid) {
			num_args -= 2;
		}

		redisAppendCommandArgv(thread_data->mp_redis_connection, num_args, args, NULL);

		redisAppendCommand(thread_data->mp_redis_connection, "ZINCRBY usermodes 1 %d", usermode.usermodeid);

		redisReply *reply;
		if(redisGetReply(thread_data->mp_redis_connection,(void**)&reply) == REDIS_OK) {
			freeReplyObject(reply);
		}
		
		if(redisGetReply(thread_data->mp_redis_connection,(void**)&reply) == REDIS_OK) {
			freeReplyObject(reply);
		}
	}


	json_t* UsermodeRecordToJson(UsermodeRecord record) {
		json_t* object = json_object();
		if(record.chanmask.length() > 0)
			json_object_set_new(object, "channelmask", json_string(record.chanmask.c_str()));
		if (record.hostmask.length() > 0)
			json_object_set_new(object, "hostmask", json_string(record.hostmask.c_str()));
		if (record.comment.length() > 0)
			json_object_set_new(object, "comment", json_string(record.comment.c_str()));
		if (record.machineid.length() > 0)
			json_object_set_new(object, "machineid", json_string(record.machineid.c_str()));

		if (record.profileid != 0)
			json_object_set_new(object, "profileid", json_integer(record.profileid));

		json_object_set_new(object, "modeflags", json_integer(record.modeflags));

		
		if(record.has_gameid == false) {
			json_object_set(object, "gameid", json_null());
		} else {
			json_object_set_new(object, "gameid", json_integer(record.gameid));
		}
		


		json_t *isGlobal = json_false();
		if(record.isGlobal) {
			isGlobal = json_true();
		}
		json_object_set(object, "isGlobal", isGlobal); //expires in seconds

		json_object_set_new(object, "expiresIn", json_integer(record.expires_at.tv_sec)); //expires in seconds

		json_object_set_new(object, "setByNick", json_string(record.setByUserSummary.nick.c_str()));

		json_object_set_new(object, "setByHost", json_string(record.setByUserSummary.hostname.c_str()));
		json_object_set_new(object, "setByPid", json_integer(record.setByUserSummary.profileid));
		return object;
	}

}