#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define CHANNEL_EXPIRE_TIME 86400
namespace Peerchat {
    const char *mp_pk_channel_name = "PEERCHATCHANID";
	int GetPeerchatChannelID(TaskThreadData *thread_data) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_channel_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}

    ChannelSummary LookupChannelById(TaskThreadData *thread_data, int channel_id) {
        ChannelSummary summary;
        Redis::Response reply;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d name", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		summary.channel_name = reply.values[0].value._str;

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d modeflags", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.basic_mode_flags = atoi(reply.values[0].value._str.c_str());
		}
		else if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			summary.basic_mode_flags = reply.values[0].value._int;
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d password", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.password = reply.values[0].value._str.c_str();
		}

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d limit", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.limit = atoi(reply.values[0].value._str.c_str());
		}
		else if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			summary.limit = reply.values[0].value._int;
		}
		else {
			summary.limit = 0;
		}


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d created_at", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}

		summary.created_at.tv_usec = 0;
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.created_at.tv_sec = atoi(reply.values[0].value._str.c_str());
		}
		else if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			summary.created_at.tv_sec = reply.values[0].value._int;
		}


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d topic", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		if(reply.values.size() > 0 && reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.topic = reply.values[0].value._str;
		}
		

		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d topic_time", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}

		summary.topic_time.tv_usec = 0;
		summary.topic_time.tv_sec = 0;
		if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.topic_time.tv_sec = atoi(reply.values[0].value._str.c_str());
		}
		else if (reply.values[0].type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			summary.topic_time.tv_sec = reply.values[0].value._int;
		}


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d topic_user", channel_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			goto error_end;
		}
		if(reply.values.size() > 0 && reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			summary.topic_user_summary = reply.values[0].value._str;
		}


		summary.channel_id = channel_id;

		summary.users = GetChannelUsers(thread_data, channel_id);

		error_end:
		return summary;
	}


	ChannelSummary CreateChannel(TaskThreadData* thread_data, std::string name) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		int channel_id = GetPeerchatChannelID(thread_data);

		ChannelSummary summary;

		summary.channel_id = channel_id;
		summary.channel_name = name;
		summary.limit = 0;
		summary.basic_mode_flags = 0;

		struct timeval curtime;
		gettimeofday(&curtime, NULL);
		summary.created_at = curtime;

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channels \"%s\" %d", name.c_str(), channel_id);

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d name \"%s\"", channel_id, name.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d modeflags 0", channel_id);

		struct timeval now;
		gettimeofday(&now, NULL);
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d created_at %d", channel_id, now.tv_sec);
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d %d", channel_id, CHANNEL_EXPIRE_TIME);

		Redis::Command(thread_data->mp_redis_connection, 0, "SET channelname_%s %d", name.c_str(), channel_id);
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channelname_%s %d", name.c_str(), CHANNEL_EXPIRE_TIME);
		return summary;
	}

	int GetChannelIdByName(TaskThreadData* thread_data, std::string name) {
		Redis::Value v;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET channelname_%s", name.c_str());
		if (Redis::CheckError(reply))
			return 0;
		if (reply.values.size() < 1)
			return 0;
		v = reply.values[0];

		int id = 0;
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			id = atoi(v.value._str.c_str());
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			id = v.value._int;
		}
		if (id != 0) {
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channelname_%s %d", name.c_str(), CHANNEL_EXPIRE_TIME);
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d %d", id, CHANNEL_EXPIRE_TIME);
		}
		return id;
	}
	ChannelSummary GetChannelSummaryByName(TaskThreadData* thread_data, std::string name, bool create) {
		int id = GetChannelIdByName(thread_data, name);
		if (id != 0) {
			return LookupChannelById(thread_data, id);
		}
		//doesn't exist, create
		if (create) {
			return CreateChannel(thread_data, name);
		}
		else {
			ChannelSummary summary;
			summary.channel_id = 0;
			summary.channel_name = name;
			return summary;
		}
	}

	void AddUserToChannel(TaskThreadData* thread_data, UserSummary user, ChannelSummary channel, int initial_flags) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY channel_%d_users 1 \"%d\"", channel.channel_id, user.id);
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d_users %d", channel.channel_id, CHANNEL_EXPIRE_TIME);

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d_user_%d modeflags %d", channel.channel_id, user.id, initial_flags);

		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d_user_%d channel_id %d", channel.channel_id, user.id, channel.channel_id);
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE channel_%d_user_%d %d", channel.channel_id, CHANNEL_EXPIRE_TIME);

		std::ostringstream id;
		id << "channel_" << channel.channel_id << "_user_" << user.id;
		user.modeflags = 0;
		ApplyUserKeys(thread_data, id.str(), user, "custkey_");

		std::ostringstream message;
		message << "\\type\\JOIN\\toChannelId\\" << channel.channel_id << "\\fromUserId\\" << user.id << "\\includeSelf\\1";
		thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, message.str().c_str());
	}

	void RemoveUserFromChannel(TaskThreadData* thread_data, UserSummary user, ChannelSummary channel, std::string type, std::string remove_message, UserSummary target, bool silent) {

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
		Redis::Command(thread_data->mp_redis_connection, 0, "ZREM channel_%d_users \"%d\"", channel.channel_id, user.id);
		Redis::Command(thread_data->mp_redis_connection, 0, "DEL channel_%d_user%d", channel.channel_id, user.id);

		if(!silent) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)remove_message.c_str(), remove_message.length());
			std::ostringstream message;
			message << "\\type\\" << type << "\\toChannelId\\" << channel.channel_id << "\\fromUserId\\" << user.id << "\\message\\" << base64 << "\\includeSelf\\1";

			if (target.id != 0) {
				message << "\\toUserId\\" << target.id;
			}
			thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, message.str().c_str());
			free((void*)base64);
		}

		///XXX: delete channel if empty & not "stay open" mode

		
    }
	int LookupUserChannelModeFlags(TaskThreadData* thread_data, int channel_id, int user_id) {
		Redis::Response reply;
		Redis::Value v;


		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d modeflags", channel_id, user_id);
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
			return 0;
		}
		v = reply.values[0];

		int modeflags = 0;
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			modeflags = atoi(v.value._str.c_str());
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			modeflags = v.value._int;
		}
		return modeflags;
	}
    std::vector<ChannelUserSummary> GetChannelUsers(TaskThreadData *thread_data, int channel_id) {
        std::vector<ChannelUserSummary> result;
        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;

        do {
			reply = Redis::Command(thread_data->mp_redis_connection, 0, "ZSCAN channel_%d_users %d", channel_id, cursor);
			if (Redis::CheckError(reply) || reply.values.size() == 0 || reply.values[0].arr_value.values.size() < 2)
				goto error_cleanup;

			v = reply.values[0].arr_value.values[0].second;
			arr = reply.values[0].arr_value.values[1].second;

		 	if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
		 		cursor = atoi(v.value._str.c_str());
		 	} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
		 		cursor = v.value._int;
		 	}
            for(size_t i=0;i<arr.arr_value.values.size();i+=2) {
                ChannelUserSummary summary;
                std::string user_id = arr.arr_value.values[i].second.value._str;
                summary.channel_id = channel_id;
                summary.user_id = atoi(user_id.c_str());
				summary.userSummary = LookupUserById(thread_data, summary.user_id);
				summary.modeflags = LookupUserChannelModeFlags(thread_data, channel_id, summary.user_id);
                result.push_back(summary);
            }
        } while(cursor != 0);
        error_cleanup:
        return result;
    }
	int GetUserModeLevel(int modeflags) {
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
		if (GetUserModeLevel(min_mode_flags) > GetUserModeLevel(from_mode_flags)) {
			peer->send_numeric(482, "You're not channel operator (Does not meet minimum rank)", false, channel);
			return false;
		}
		if (GetUserModeLevel(to_mode_flags) > GetUserModeLevel(from_mode_flags)) {
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
}