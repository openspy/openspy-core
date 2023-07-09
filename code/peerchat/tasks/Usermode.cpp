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
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HMGET %s id chanmask hostname comment machineid profileid modeflags gameid expiresAt setAt setBy setByHost setByPid setByNick", cacheKey.c_str());

		record.usermodeid = atoi(reply->element[0]->str);
		record.chanmask = reply->element[1]->str;
		record.hostmask = reply->element[2]->str;
		record.comment = reply->element[3]->str;
		record.machineid = reply->element[4]->str;
		record.profileid = atoi(reply->element[5]->str);
		record.modeflags = atoi(reply->element[6]->str);

		record.has_gameid = false;
		if(reply->element[7]->type == REDIS_REPLY_STRING) {
			record.gameid = atoi(reply->element[7]->str);
			record.has_gameid = true;
		}

		record.expires_at.tv_sec = atoi(reply->element[8]->str);
		record.set_at.tv_sec = atoi(reply->element[9]->str);

		if(reply->element[10]->type == REDIS_REPLY_STRING) {
			record.setByUserSummary = UserSummary(reply->element[10]->str);
		} else {
			record.setByUserSummary.hostname = reply->element[11]->str;
			record.setByUserSummary.profileid = atoi(reply->element[12]->str);
			record.setByUserSummary.nick = reply->element[13]->str;
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

		ss << "HMSET " << key_name;
		ss << "id \"" << usermode.usermodeid << "\"";
		ss << "chanmask \"" << usermode.chanmask << "\"";
		ss << "hostmask \"" << usermode.hostmask << "\"";
		ss << "comment \"" << usermode.comment << "\"";
		ss << "machineid \"" << usermode.machineid << "\"";
		ss << "profileid \"" << usermode.profileid << "\"";
		ss << "modeflags \"" << usermode.modeflags << "\"";

		if(usermode.has_gameid) {
			ss << "gameid \"" << usermode.gameid << "\"";
		}

		ss << "isGlobal \"" << (usermode.usermodeid > 0) << "\"";
		ss << "setByNick \"" << usermode.setByUserSummary.nick.c_str() << "\"";
		ss << "setByHost \"" << usermode.setByUserSummary.hostname.c_str() << "\"";
		ss << "setByPid \"" << usermode.setByUserSummary.profileid << "\"";
		ss << "setAt \"" << usermode.set_at.tv_sec << "\"";
		ss << "expiresAt \"" << usermode.expires_at.tv_sec << "\"";

		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, ss.str().c_str());
		freeReplyObject(reply);

		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "ZINCRBY usermodes 1 %d", usermode.usermodeid);
		freeReplyObject(reply);
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