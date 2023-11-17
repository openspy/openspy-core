#include "tasks.h"
#include <sstream>
#include <cctype>
#include <server/Server.h>

#include <OS/HTTP.h>


namespace Peerchat {
    const char *mp_pk_channel_name = "PEERCHATCHANID";
	int GetPeerchatChannelID(TaskThreadData *thread_data) {
		int ret = -1;

        redisReply *reply;

		redisAppendCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
		redisAppendCommand(thread_data->mp_redis_connection, "INCR %s", mp_pk_channel_name);
		
		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
		freeReplyObject(reply);

		redisGetReply(thread_data->mp_redis_connection,(void**)&reply);	
		ret = reply->integer;
		freeReplyObject(reply);
		
		return ret;
	}

    ChannelSummary LookupChannelById(TaskThreadData *thread_data, int channel_id) {
        ChannelSummary summary;

		std::ostringstream ss;
		//ss << "HMGET channel_" << channel_id << " name entrymsg modeflags password limit created_at topic topic_time topic_user";
		ss << "channel_" << channel_id;

		std::string channel_name = ss.str();
		const char *args[] = {
			"HMGET", channel_name.c_str(), "name", "entrymsg", "modeflags", "password", "limit", "created_at", "topic", "topic_time", "topic_user"
		};

		redisReply *reply = (redisReply *)redisCommandArgv(thread_data->mp_redis_connection, sizeof(args) / sizeof(const char *), args, NULL);
        
		if(reply) {
            summary.channel_id = channel_id;
            if(reply->element[0]->type == REDIS_REPLY_STRING) {
                summary.channel_name = reply->element[0]->str;
            }
            
            if(reply->element[1]->type == REDIS_REPLY_STRING) {
                summary.entrymsg = reply->element[1]->str;
            }
            
            if(reply->element[2]->type == REDIS_REPLY_STRING) {
                summary.basic_mode_flags = atoi(reply->element[2]->str);
            }
            
            if(reply->element[3]->type == REDIS_REPLY_STRING) {
                summary.password = reply->element[3]->str;
            }
            
            if(reply->element[4]->type == REDIS_REPLY_STRING) {
                summary.limit = atoi(reply->element[4]->str);
            }
            
            if(reply->element[5]->type == REDIS_REPLY_STRING) {
                summary.created_at.tv_sec = atoi(reply->element[5]->str);
            }
            
            if(reply->element[6]->type == REDIS_REPLY_STRING) {
                summary.topic = reply->element[6]->str;
            }
            
            if(reply->element[7]->type == REDIS_REPLY_STRING) {
                summary.topic_time.tv_sec = atoi(reply->element[7]->str);
            }
            
            if(reply->element[8]->type == REDIS_REPLY_STRING) {
                summary.topic_user_summary = reply->element[8]->str;
            }			
			freeReplyObject(reply);
            summary.users = GetChannelUsers(thread_data, summary.channel_id);
		}
		return summary;
	}
	void AssociateUsermodeToChannel(UsermodeRecord record, ChannelSummary summary, TaskThreadData* thread_data) {
		std::ostringstream ss;
		ss << "channel_" << summary.channel_id << "_usermodes";

		std::string channel = ss.str();

		int num_redis_cmds = 0;
		redisReply *reply;
		
		redisAppendCommand(thread_data->mp_redis_connection, "HSET %s %d %s", channel.c_str(), record.usermodeid, record.chanmask.c_str()); num_redis_cmds++;
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel.c_str(), CHANNEL_EXPIRE_TIME); num_redis_cmds++;

		for (int i = 0; i < num_redis_cmds; i++) {
			int r = redisGetReply(thread_data->mp_redis_connection, (void**)&reply);
			if(r == REDIS_OK) {
				freeReplyObject(reply);
			}
		}
	}
	void LoadUsermodesForChannel(ChannelSummary summary, TaskThreadData* thread_data) {
		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelmask", json_string(summary.channel_name.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode/lookup";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, NULL);

		free(json_data);
		json_decref(send_json);

		
		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskShared::WebErrorDetails error_info;

		if (!TaskShared::Handle_WebError(send_json, error_info)) {
			size_t num_items = json_array_size(send_json);
			for (int i = 0; i < num_items; i++) {
				json_t* item = json_array_get(send_json, i);

				UsermodeRecord record = GetUsermodeFromJson(item);
				WriteUsermodeToCache(record, thread_data);
				AssociateUsermodeToChannel(record, summary, thread_data);
			}
		}
		if(send_json)
			json_decref(send_json);
	}
	ChannelSummary CreateChannel(TaskThreadData* thread_data, std::string name) {
		//Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		redisReply *reply;

		std::string formatted_name;
		std::transform(name.begin(),name.end(),std::back_inserter(formatted_name),tolower);

		int channel_id = GetPeerchatChannelID(thread_data);

		ChannelSummary summary;

		summary.channel_id = channel_id;
		summary.channel_name = name;
		summary.limit = 0;
		summary.basic_mode_flags = 0;

		

		uv_timespec64_t curtime;
		uv_clock_gettime(UV_CLOCK_REALTIME , &curtime);
		summary.created_at = curtime;

		int total_redis_calls = 0;

		std::ostringstream ss;
		ss << "channel_" << channel_id;
		std::string channel = ss.str();

		ss.str("");
		ss << "channelname_" << formatted_name.c_str();
		std::string channel_name = ss.str();

		redisAppendCommand(thread_data->mp_redis_connection, "HSET channels %s %d", formatted_name.c_str(), channel_id); total_redis_calls++;

		ss.str("");
		ss << curtime.tv_sec;
		std::string now_str = ss.str();

		const char *args[] = {
			"HMSET", channel.c_str(), "name", name.c_str(), "modeflags", "0", "created_at", now_str.c_str()
		};
		redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(args) / sizeof(const char *), args, NULL); total_redis_calls++;

		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel.c_str(), CHANNEL_EXPIRE_TIME); total_redis_calls++;
		redisAppendCommand(thread_data->mp_redis_connection, "SET %s %d", channel_name.c_str(), channel_id); total_redis_calls++;
		//redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel_name.c_str(), CHANNEL_EXPIRE_TIME); total_redis_calls++; //don't expire this, since the keepalive logic is harder than the performance hit of misses for this

		for(int i=0;i<total_redis_calls;i++) {
			if(redisGetReply(thread_data->mp_redis_connection,(void**)&reply) == REDIS_OK) {
				freeReplyObject(reply);
			}			
		}
		
		//load chanprops & usermodes
		ApplyChanProps(summary);

		summary = LookupChannelById(thread_data, channel_id);

		LoadUsermodesForChannel(summary, thread_data);
		return summary;
	}

	int GetChannelIdByName(TaskThreadData* thread_data, std::string name) {
		int id = 0;

		std::ostringstream ss;
		ss << "channelname_" << name.c_str();
		std::string channel_name = ss.str();

		bool do_delete = false;

		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "GET %s", channel_name.c_str());

		if(reply->type == REDIS_REPLY_STRING) {
			id = atoi(reply->str);
		} else if(reply->type == REDIS_REPLY_INTEGER) {
			id = reply->integer;
		}
		freeReplyObject(reply);
		if (id != 0) {            
			ss.str("");
			ss << "channel_" << id;
			std::string channel = ss.str();

			redisAppendCommand(thread_data->mp_redis_connection, "EXISTS %s", channel.c_str());            
			redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel.c_str(), CHANNEL_EXPIRE_TIME);

			int r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				if(reply->integer != 1) { //cache miss
					id = 0;
					do_delete = true;
				}
				freeReplyObject(reply);
			}

			r = redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			if(r == REDIS_OK) {
				freeReplyObject(reply);
			}
		}

		if(do_delete) {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "DEL %s", channel_name.c_str());
			if(reply) {
				freeReplyObject(reply);
			}
		}
		return id;
	}
	ChannelSummary GetChannelSummaryByName(TaskThreadData* thread_data, std::string name, bool create, UserSummary creator, bool *created) {
		std::string formatted_name;
		std::transform(name.begin(),name.end(),std::back_inserter(formatted_name),tolower);
		int id = GetChannelIdByName(thread_data, formatted_name);
		if (id != 0) {
			return LookupChannelById(thread_data, id);
		}
		//doesn't exist, create
		if (create) {
			if(created != NULL)
				*created = true;
			return CreateChannel(thread_data, formatted_name);
		}
		else {
			if(created != NULL)
				*created = false;
			ChannelSummary summary;
			summary.channel_id = 0;
			summary.channel_name = name;
			return summary;
		}
	}

	void AddUserToChannel(TaskThreadData* thread_data, UserSummary user, ChannelSummary channel, int initial_flags) {
		std::ostringstream ss;
		ss << "channel_" << channel.channel_id << "_users";
		std::string channel_users_key = ss.str();

		ss.str("");
		ss << "channel_" << channel.channel_id << "_user_" << user.id;
		std::string channel_user_key = ss.str();

		int total_redis_calls = 0;

		std::string user_channels_key;
		ss.str("");
		ss << "user_" << user.id << "_channels";
		user_channels_key = ss.str();
		redisAppendCommand(thread_data->mp_redis_connection, "ZADD %s 1 %d", user_channels_key.c_str(), channel.channel_id); total_redis_calls++;

		redisAppendCommand(thread_data->mp_redis_connection, "ZINCRBY %s 1 %d", channel_users_key.c_str(), user.id); total_redis_calls++;

		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", user_channels_key.c_str(), USER_EXPIRE_TIME); total_redis_calls++;
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel_users_key.c_str(), CHANNEL_EXPIRE_TIME); total_redis_calls++;

		std::string modeflags_str, channelid_str;
		ss.str("");
		ss << initial_flags;
		modeflags_str = ss.str();

		ss.str("");
		ss << channel.channel_id;
		channelid_str = ss.str();

		const char *args[] = {
			"HMSET", channel_user_key.c_str(), "modeflags", modeflags_str.c_str(), "channel_id", channelid_str.c_str()
		};
		redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(args) / sizeof(const char *), args, NULL); total_redis_calls++;
		

		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel_user_key.c_str(), CHANNEL_EXPIRE_TIME);  total_redis_calls++;

		void *reply;
		for(int i=0;i<total_redis_calls;i++) {
			redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			freeReplyObject(reply);
		}

		std::ostringstream id;
		id << "channel_" << channel.channel_id << "_user_" << user.id;
		user.modeflags = 0;
		ApplyUserKeys(thread_data, id.str(), user, "_custkeys");

		SendUpdateUserChanModeflags(thread_data, channel.channel_id, user.id, initial_flags, 0);

		std::ostringstream message;
		message << "\\type\\JOIN\\toChannelId\\" << channel.channel_id << "\\fromUserSummary\\" << user.ToBase64String(true) << "\\includeSelf\\1";
		sendAMQPMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, message.str().c_str());

	}

	void RemoveUserFromChannel(TaskThreadData* thread_data, UserSummary user, ChannelSummary channel, std::string type, std::string remove_message, UserSummary target, bool silent, int requiredChanUserModes) {
		std::ostringstream ss;
		ss << "channel_" << channel.channel_id << "_users";
		std::string channel_users_key = ss.str();

		ss.str("");
		ss << "channel_" << channel.channel_id << "_user_" << user.id;
		std::string channel_user_key = ss.str();

		std::ostringstream message;
		redisReply *reply;

		int num_redis_cmds = 0;

		if (target.id != 0) {
			redisAppendCommand(thread_data->mp_redis_connection, "ZREM %s %d", channel_users_key.c_str(), target.id); num_redis_cmds++;
			redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", channel_user_key.c_str()); num_redis_cmds++;
			redisAppendCommand(thread_data->mp_redis_connection, "DEL %s_custkeys", channel_user_key.c_str()); num_redis_cmds++;
		} else {
			redisAppendCommand(thread_data->mp_redis_connection, "ZREM %s %d", channel_users_key.c_str(), user.id); num_redis_cmds++;
			redisAppendCommand(thread_data->mp_redis_connection, "DEL %s", channel_user_key.c_str()); num_redis_cmds++;
			redisAppendCommand(thread_data->mp_redis_connection, "DEL %s_custkeys", channel_user_key.c_str()); num_redis_cmds++;
		}

		std::string user_channels_key;
		ss.str("");
		ss << "user_" << user.id << "_channels";
		user_channels_key = ss.str();
		redisAppendCommand(thread_data->mp_redis_connection, "ZREM %s %d", channel_users_key.c_str(), channel.channel_id); num_redis_cmds++;

		for(int i=0;i<num_redis_cmds;i++) {
			redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
			freeReplyObject(reply);
		}

		int old_modeflags = LookupUserChannelModeFlags(thread_data, channel.channel_id, user.id);

		if(!silent) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)remove_message.c_str(), remove_message.length());			
			message << "\\type\\" << type << "\\toChannelId\\" << channel.channel_id << "\\fromUserSummary\\" << user.ToBase64String(true) << "\\message\\" << base64 << "\\includeSelf\\1";

			message << "\\requiredChanUserModes\\" << requiredChanUserModes;

			if (target.id != 0) {
				message << "\\toUserSummary\\" << target.ToBase64String(true);
			}
			sendAMQPMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, message.str().c_str());
			free((void*)base64);
		}

		if (!(channel.basic_mode_flags & EChannelMode_StayOpen)) {
			///XXX: delete channel if empty & not "stay open" mode
			int count = CountChannelUsers(thread_data, channel.channel_id);
			if (count == 0) {
				DeleteChannelById(thread_data, channel.channel_id);
			}
		}
		SendUpdateUserChanModeflags(thread_data, channel.channel_id, target.id, 0, old_modeflags);
    }

	int LookupUserChannelModeFlags(TaskThreadData* thread_data, int channel_id, int user_id) {
		std::ostringstream ss;
		ss << "channel_" << channel_id << "_user_" << user_id;
		std::string channel_user_key = ss.str();

		int modeflags = 0;
		redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s modeflags", channel_user_key.c_str());

		if(reply) {
			if(reply->type == REDIS_REPLY_INTEGER) {
				modeflags = reply->integer;
			} else if(reply->type == REDIS_REPLY_STRING) {
				modeflags = atoi(reply->str);
			}
			freeReplyObject(reply);
		}
		return modeflags;
	}
    int CountChannelUsers(TaskThreadData *thread_data, int channel_id) {
		int count = 0;
        int cursor = 0;

		redisReply *reply;

		std::ostringstream ss;
		ss << "channel_" << channel_id << "_users";
		std::string channel_users_key = ss.str();

		do {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d", channel_users_key.c_str(), cursor);

			if (reply == NULL)
				break;

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			count += reply->element[1]->elements / 2;

			freeReplyObject(reply);
		} while(cursor != 0);

        return count;
    }
	void DeleteChannelById(TaskThreadData *thread_data, int channel_id) {
		ChannelSummary channel = LookupChannelById(thread_data, channel_id);

		std::string channel_key, channelname_key, channel_usermodes;
		std::ostringstream ss;
		ss << "channelname_" << channel.channel_name;
		channelname_key = ss.str();

		ss.str("");
		ss << "channel_" << channel_id;
		channel_key = ss.str();

		ss << "_usermodes";
		channel_usermodes = ss.str();

		
		redisReply *reply;	
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", channel_key.c_str(), CHANNEL_DELETE_EXPIRE_TIME); //keep basic information around for short time (for PART events, etc)
		freeReplyObject(reply);
		
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "DEL %s", channelname_key.c_str());
		freeReplyObject(reply);
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HDEL channels %s", channel.channel_name.c_str());
		freeReplyObject(reply);

		DeleteTemporaryUsermodesForChannel(thread_data, channel);
		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "DEL %s", channel_usermodes.c_str());
		freeReplyObject(reply);
	}
    std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id) {
        std::vector<ChannelUserSummary> result;

		std::string chan_users_str;
		std::ostringstream ss;
		ss << "channel_" << channel_id << "_users";
		chan_users_str = ss.str();
        
        int cursor = 0;
		redisReply *reply;

        do {
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "ZSCAN %s %d", chan_users_str.c_str(), cursor);

            if (reply == NULL || thread_data->mp_redis_connection->err || reply->elements < 2) {
				if(reply) {
					freeReplyObject(reply);
				}
                return result;
            }
		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			for(size_t i = 0; i < reply->element[1]->elements;i+=2) {
				ChannelUserSummary summary;
				std::string user_id = reply->element[1]->element[i]->str;
                summary.channel_id = channel_id;
                summary.user_id = atoi(user_id.c_str());
				summary.userSummary = LookupUserById(thread_data, summary.user_id);

				if(summary.userSummary.id == 0) {
					continue;
				}
				summary.modeflags = LookupUserChannelModeFlags(thread_data, channel_id, summary.user_id);
                result.push_back(summary);
			}

			freeReplyObject(reply);

        } while(cursor != 0);

        return result;
    }
	int GetUserChannelModeLevel(int modeflags) {
		if (modeflags & EUserChannelFlag_Owner) {
			return 4;
		}
		if (modeflags & EUserChannelFlag_Op) {
			return 3;
		}
		if (modeflags & EUserChannelFlag_HalfOp) {
			return 2;
		}
		if (modeflags & EUserChannelFlag_Voice) {
			return 1;
		}
		return 0;
	}
	bool CheckActionPermissions(Peer *peer, std::string channel, int from_mode_flags, int to_mode_flags, int min_mode_flags) {
		if (peer->GetOperFlags() & OPERPRIVS_OPEROVERRIDE) {
			return true;
		}
		if (GetUserChannelModeLevel(min_mode_flags) > GetUserChannelModeLevel(from_mode_flags)) {
			peer->send_numeric(482, "You're not channel operator (Does not meet minimum rank)", false, channel);
			return false;
		}
		if (GetUserChannelModeLevel(to_mode_flags) > GetUserChannelModeLevel(from_mode_flags)) {
			peer->send_numeric(482, "You're not channel operator (Target user ranked higher)", false, channel);
			return false;
		}
		return true;
	}
	EUserChannelFlag GetMinimumModeFlagsFromUpdateSet(int update_mode_flags) {
		if (update_mode_flags & EUserChannelFlag_Owner) {
			return EUserChannelFlag_Owner;
		}
		if (update_mode_flags & EUserChannelFlag_Op) {
			return EUserChannelFlag_Op;
		}
		if (update_mode_flags & EUserChannelFlag_HalfOp) {
			return EUserChannelFlag_HalfOp;
		}
		if (update_mode_flags & EUserChannelFlag_Voice) {
			return EUserChannelFlag_Voice;
		}
		return EUserChannelFlag_Owner;
	}
	bool TestChannelUserModeChangeItem(TaskThreadData* thread_data, Peer* peer, ChannelSummary channel_summary, std::string target_username, int from_mode_flags, int update_flags) {
		UserSummary target_summary = GetUserSummaryByName(thread_data, target_username);
		if(target_summary.id == 0) {
			peer->send_no_such_target_error(target_username);
			return false;
		}

		if (peer->GetOperFlags() & OPERPRIVS_OPEROVERRIDE) {
			return true;
		}

		int to_mode_flags = LookupUserChannelModeFlags(thread_data, channel_summary.channel_id, target_summary.id);
		if(!CheckActionPermissions(peer, channel_summary.channel_name, from_mode_flags, to_mode_flags, (int)GetMinimumModeFlagsFromUpdateSet(to_mode_flags))) {
			return false;
		}
		return true;
	}
	bool CheckChannelUserModeChange(TaskThreadData* thread_data, Peer *peer, std::string channel, int from_mode_flags, std::map<std::string, int> set_usermodes, std::map<std::string, int> unset_usermodes) {
		ChannelSummary channel_summary = GetChannelSummaryByName(thread_data, channel, false);

		if (peer->GetOperFlags() & OPERPRIVS_OPEROVERRIDE) {
			return true;
		}

		std::map<std::string, int>::iterator it = set_usermodes.begin();
		while (it != set_usermodes.end()) {
			std::pair<std::string, int> p = *it;
			if (!TestChannelUserModeChangeItem(thread_data, peer, channel_summary, p.first, from_mode_flags, p.second)) {
				return false;
			}
			it++;
		}

		it = unset_usermodes.begin();
		while (it != unset_usermodes.end()) {
			std::pair<std::string, int> p = *it;
			if (!TestChannelUserModeChangeItem(thread_data, peer, channel_summary, p.first, from_mode_flags, p.second)) {
				return false;
			}
			it++;
		}
		return true;
	}
	void SendUpdateUserChanModeflags(TaskThreadData* thread_data, int channel_id, int user_id, int modeflags, int old_modeflags) {
		std::ostringstream message;
		message << "\\type\\UPDATE_USER_CHANMODEFLAGS\\channel_id\\" << channel_id << "\\user_id\\" << user_id << "\\modeflags\\" << modeflags << "\\old_modeflags\\" << old_modeflags;
		sendAMQPMessage(peerchat_channel_exchange, peerchat_key_updates_routingkey, message.str().c_str());		
	}
}
