#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	void HandleSetUsermodes(TaskThreadData* thread_data, PeerchatBackendRequest request, std::ostringstream &ss, bool unset) {
		std::map<std::string, int>::iterator it = request.channel_modify.set_usermodes.begin();
		if (unset)
			it = request.channel_modify.unset_usermodes.begin();
		std::ostringstream user_ss;
		while (it != (unset ? request.channel_modify.unset_usermodes.end() : request.channel_modify.set_usermodes.end())) {
			std::pair<std::string, int> p = *(it++);
			std::string name = p.first;
			int modes = p.second;

			UserSummary summary = GetUserSummaryByName(thread_data, name);

			if (summary.id == 0) continue;

			if (modes & EUserChannelFlag_Voice) {
				ss << "v";
				user_ss << name;
			}
			
			if (modes & EUserChannelFlag_Op || modes & EUserChannelFlag_HalfOp || modes & EUserChannelFlag_Owner) {
				ss << "o";
				user_ss << name;
			}

			Redis::Response reply;
			Redis::Value v;


			reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d modeflags", request.channel_summary.channel_id, summary.id);
			if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
				goto error_end;
			}
			v = reply.values[0];

			int modeflags = 0;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				modeflags = atoi(v.value._str.c_str());
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				modeflags = v.value._int;
			}
			if(unset) {
				modeflags &= ~modes;
			}
			else {
				modeflags |= modes;
			}
			
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d_user_%d modeflags %d", request.channel_summary.channel_id, summary.id, modeflags);

		}
		error_end:

		ss << " " << user_ss.str();
	}
    bool Perform_UpdateChannelModes(PeerchatBackendRequest request, TaskThreadData *thread_data) {

		std::string target;
		if (request.channel_summary.channel_name.length() > 0) {
			target = request.channel_summary.channel_name;
		}
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		int from_mode_flags = LookupUserChannelModeFlags(thread_data, summary.channel_id, request.peer->GetBackendId());
		if (summary.channel_id == 0 || !CheckActionPermissions(request.peer, request.channel_summary.channel_name, from_mode_flags, 0, (int)EUserChannelFlag_HalfOp) || !CheckChannelUserModeChange(thread_data, request.peer, request.channel_summary.channel_name, from_mode_flags, request.channel_modify.set_usermodes, request.channel_modify.unset_usermodes)) {
			if (summary.channel_id == 0) {
				response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
			}
			else {
				response.error_details.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
			}
			
			if (request.callback) {
				request.callback(response, request.peer);
			}
			if (request.peer) {
				request.peer->DecRef();
			}
			return true;
		}
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);

		std::ostringstream mode_message;

		request.channel_summary = summary;

		if (request.channel_modify.set_mode_flags != 0 || request.channel_modify.update_password || request.channel_modify.update_limit || request.channel_modify.set_usermodes.size()) {
			bool added_mode = false;
			for (int i = 0; i < num_channel_mode_flags; i++) {
				if (request.channel_modify.set_mode_flags & channel_mode_flag_map[i].flag) {
					if (~summary.basic_mode_flags & channel_mode_flag_map[i].flag) {
						if (added_mode == false) {
							mode_message << "+";
							added_mode = true;
						}
						mode_message << channel_mode_flag_map[i].character;
						summary.basic_mode_flags |= channel_mode_flag_map[i].flag;
						
					}
				}
			}

			if (request.channel_modify.set_usermodes.size()) {
				if (added_mode == false) {
					mode_message << "+";
					added_mode = true;
				}
				HandleSetUsermodes(thread_data, request, mode_message, false);
			}

			if (request.channel_modify.update_password && request.channel_modify.password.length() != 0) {
				if (added_mode == false) {
					mode_message << "+";
					added_mode = true;
				}
				mode_message << "k";
			}

			if (request.channel_modify.update_limit && request.channel_modify.limit != 0) {
				if (added_mode == false) {
					mode_message << "+";
					added_mode = true;
				}
				mode_message << "l";
			}

			if (request.channel_modify.update_password && request.channel_modify.password.length() != 0) {
				mode_message << " " << request.channel_modify.password;
			}

			if (request.channel_modify.update_limit && request.channel_modify.limit != 0) {
				mode_message << " " << request.channel_modify.limit;
			}
		}


		if (request.channel_modify.unset_mode_flags != 0 || request.channel_modify.update_password || request.channel_modify.update_limit) {
			bool removed_mode = false;
			for (int i = 0; i < num_channel_mode_flags; i++) {
				if (request.channel_modify.unset_mode_flags & channel_mode_flag_map[i].flag) {
					if (summary.basic_mode_flags & channel_mode_flag_map[i].flag) {
						if (removed_mode == false) {
							mode_message << "-";
							removed_mode = true;
						}
						mode_message << channel_mode_flag_map[i].character;
						summary.basic_mode_flags &= ~channel_mode_flag_map[i].flag;
					}
				}
			}
			if (request.channel_modify.update_password && request.channel_modify.password.length() == 0) {
				if (removed_mode == false) {
					mode_message << "-";
					removed_mode = true;
				}
				mode_message << "k";
			}

			if (request.channel_modify.update_limit && request.channel_modify.limit == 0) {
				if (removed_mode == false) {
					mode_message << "-";
					removed_mode = true;
				}
				mode_message << "l";
			}

			if (request.channel_modify.unset_usermodes.size()) {
				if (removed_mode == false) {
					mode_message << "-";
					removed_mode = true;
				}
				HandleSetUsermodes(thread_data, request, mode_message, true);
			}
		}

		
		Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d modeflags %d", summary.channel_id, summary.basic_mode_flags);

		if (request.channel_modify.update_limit && request.channel_modify.limit != 0) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d limit %d", summary.channel_id, request.channel_modify.limit);
		}
		else if(request.channel_modify.update_limit){
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL channel_%d limit", summary.channel_id);
		}
		if (request.channel_modify.update_password && request.channel_modify.password.length() != 0) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d password \"%s\"", summary.channel_id, request.channel_modify.password.c_str());
		}
		else if (request.channel_modify.update_password) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL channel_%d password", summary.channel_id);
		}

		if(request.channel_modify.update_topic && request.channel_modify.topic.length() != 0) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d topic \"%s\"", summary.channel_id, request.channel_modify.topic.c_str());
			struct timeval now;
			gettimeofday(&now, NULL);
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d topic_time %d", summary.channel_id, now.tv_sec);
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d topic_user \"%s\"", summary.channel_id, request.peer->GetUserDetails().ToString().c_str());
		} else if(request.channel_modify.update_topic) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL channel_%d topic", summary.channel_id);
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL channel_%d topic_time", summary.channel_id);
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL channel_%d topic_user", summary.channel_id);
		}

		const char *base64 = OS::BinToBase64Str((uint8_t *)mode_message.str().c_str(), mode_message.str().length());
		std::string b64_string = base64;
		free((void *)base64);


		if (mode_message.str().size()) {
			std::ostringstream mq_message;
			mq_message << "\\type\\MODE\\toChannelId\\" << summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id << "\\includeSelf\\1";

			thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
		}

		if (request.channel_modify.topic.size()) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)request.channel_modify.topic.c_str(), request.channel_modify.topic.length());
			b64_string = base64;
			free((void*)base64);


			std::ostringstream mq_message;
			mq_message << "\\type\\TOPIC\\toChannelId\\" << summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id << "\\includeSelf\\1";

			thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
		}
        
		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}