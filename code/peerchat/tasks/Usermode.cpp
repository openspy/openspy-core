#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	const char *mp_pk_temporary_usermode_id = "USERMODEID";
	int GetTemporaryUsermodeId(TaskThreadData *thread_data) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_temporary_usermode_id);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
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
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		Redis::Response reply;
		Redis::Value v;


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s id", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.usermodeid = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s chanmask", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.chanmask = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s hostmask", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.hostmask = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s comment", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.comment = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s machineid", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.machineid = OS::strip_quotes(v.value._str).c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s profileid", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.profileid = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s modeflags", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.modeflags = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s gameid", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.gameid = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s expiresAt", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.expires_at.tv_sec = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setAt", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.set_at.tv_sec = atoi(OS::strip_quotes(v.value._str).c_str());
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setBy", cacheKey.c_str());
		if (Redis::CheckError(reply)) {
			goto end_error;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			record.setByUserSummary = UserSummary(OS::strip_quotes(v.value._str).c_str());
		} else {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setByHost", cacheKey.c_str());
			if (Redis::CheckError(reply)) {
				goto end_error;
			}
			v = reply.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				record.setByUserSummary.hostname = OS::strip_quotes(v.value._str).c_str();
			}

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setByPid", cacheKey.c_str());
			if (Redis::CheckError(reply)) {
				goto end_error;
			}
			v = reply.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				record.setByUserSummary.profileid = atoi(OS::strip_quotes(v.value._str).c_str());
			}	

			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s setByNick", cacheKey.c_str());
			if (Redis::CheckError(reply)) {
				goto end_error;
			}
			v = reply.values.front();
			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				record.setByUserSummary.nick = OS::strip_quotes(v.value._str).c_str();
			}			
		}

		end_error:
		return;
	}
	bool UsermodeMatchesUser(UsermodeRecord usermode, UserSummary summary) {
		if(usermode.profileid != 0 && summary.profileid == usermode.profileid) {
			return true;
		}

		if(usermode.hostmask.length() > 0 && match2(usermode.hostmask.c_str(), summary.hostname.c_str())) {
			return true;
		}

		if(usermode.machineid.length() > 0 && match2(usermode.machineid.c_str(), summary.realname.c_str())) {
			return true;
		}

		if(usermode.gameid != -1 && usermode.gameid == summary.gameid) {
			return true;
		}
		return false;
	}
	int getEffectiveUsermode(TaskThreadData* thread_data, int channel_id, UserSummary summary, Peer *peer) {

		int modeflags = 0;

		Redis::Response reply;
		Redis::Value v, arr;

		UsermodeRecord record;

		std::string keyname;

		int cursor = 0;
		//scan channel usermodes
		do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN channel_%d_usermodes %d match *", channel_id, cursor);
			if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR || reply.values[0].arr_value.values.size() < 2)
				goto error_cleanup;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;
			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
				if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.value._int;
				}

				if(arr.arr_value.values.size() <= 0) {
					break;
				}

				for(size_t i=0;i<arr.arr_value.values.size();i++) {
					if(i % 2 == 0) {
						keyname = "USERMODE_" + arr.arr_value.values[i].second.value._str;
						LoadUsermodeFromCache(thread_data, keyname, record);
						if(UsermodeMatchesUser(record, summary)) {
							modeflags |= record.modeflags;
						}
						
					}
						
				}
			}
		} while(cursor != 0);


		/*
			No permitted gameid match found, therefore user is banned
		*/
		if((modeflags & EUserChannelFlag_GameidPermitted) == 0) {
			modeflags |= EUserChannelFlag_Banned;
		}

		error_cleanup:
		return modeflags;
	}

	void WriteUsermodeToCache(UsermodeRecord usermode, TaskThreadData* thread_data) { 
		

		std::ostringstream s;
		s << "USERMODE_" << usermode.usermodeid;
		std::string key_name = s.str();

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", key_name.c_str(), usermode.usermodeid);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s chanmask \"%s\"", key_name.c_str(), usermode.chanmask.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s hostmask \"%s\"", key_name.c_str(), usermode.hostmask.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s comment \"%s\"", key_name.c_str(), usermode.comment.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s machineid \"%s\"", key_name.c_str(), usermode.machineid.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s profileid %d", key_name.c_str(), usermode.profileid);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s modeflags %d", key_name.c_str(), usermode.modeflags);
		if(usermode.has_gameid) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", key_name.c_str(), usermode.gameid);
		}
			
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s isGlobal %d", key_name.c_str(), usermode.usermodeid > 0);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setByNick \"%s\"", key_name.c_str(), usermode.setByUserSummary.nick.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setByHost \"%s\"", key_name.c_str(), usermode.setByUserSummary.hostname.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setByPid \"%d\"", key_name.c_str(), usermode.setByUserSummary.profileid);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s setAt %d", key_name.c_str(), usermode.set_at.tv_sec);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s expiresAt %d", key_name.c_str(), usermode.expires_at.tv_sec);

		Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY usermodes 1 %d", usermode.usermodeid);
	}


	json_t* UsermodeRecordToJson(UsermodeRecord record) {
		json_t* object = json_object();
		if(record.chanmask.length() > 0)
			json_object_set(object, "channelmask", json_string(record.chanmask.c_str()));
		if (record.hostmask.length() > 0)
			json_object_set(object, "hostmask", json_string(record.hostmask.c_str()));
		if (record.comment.length() > 0)
			json_object_set(object, "comment", json_string(record.comment.c_str()));
		if (record.machineid.length() > 0)
			json_object_set(object, "machineid", json_string(record.machineid.c_str()));

		if (record.profileid != 0)
			json_object_set(object, "profileid", json_integer(record.profileid));

		json_object_set(object, "modeflags", json_integer(record.modeflags));

		json_t *gameid_item = json_integer(record.gameid);
		if(record.has_gameid == false) {
			gameid_item = json_null();
		}
		json_object_set(object, "gameid", gameid_item);

		json_object_set(object, "expiresIn", json_integer(record.expires_at.tv_sec)); //expires in seconds

		json_object_set(object, "setByNick", json_string(record.setByUserSummary.nick.c_str()));

		json_object_set(object, "setByHost", json_string(record.setByUserSummary.hostname.c_str()));
		json_object_set(object, "setByPid", json_integer(record.setByUserSummary.profileid));
		return object;
	}

}