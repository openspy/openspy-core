#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	void HandleSetUsermodes(TaskThreadData* thread_data, PeerchatBackendRequest request, std::ostringstream &ss, bool unset) {
		std::ostringstream message;

		std::map<std::string, int>::iterator it = request.channel_modify.set_usermodes.begin();
		if (unset)
			it = request.channel_modify.unset_usermodes.begin();
		std::ostringstream user_ss;
		while (it != (unset ? request.channel_modify.unset_usermodes.end() : request.channel_modify.set_usermodes.end())) {
			std::pair<std::string, int> p = *(it++);
			std::string name = p.first;
			int modes = p.second;

			UserSummary summary = GetUserSummaryByName(thread_data, name);

			std::ostringstream chan_ss;
			chan_ss << "channel_" << request.channel_summary.channel_id << "_user_" << summary.id;
			std::string chan_user_key = chan_ss.str();

			if (summary.id == 0) continue;

			if (modes & EUserChannelFlag_Voice) {
				ss << "v";
				user_ss << name << " ";
			}
			
			if (modes & EUserChannelFlag_Op || modes & EUserChannelFlag_HalfOp || modes & EUserChannelFlag_Owner) {
				ss << "o";
				user_ss << name << " ";
			}

			redisReply *reply;

			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s modeflags", chan_user_key.c_str());
			if (reply == NULL) {
				goto error_end;
			}
			
			int modeflags = 0, original_modeflags;
			if (reply->type == REDIS_REPLY_STRING) {
				modeflags = atoi(reply->str);
			}
			else if (reply->type == REDIS_REPLY_INTEGER) {
				modeflags = reply->integer;
			}

			freeReplyObject(reply);

			original_modeflags = modeflags;

			if(unset) {
				modeflags &= ~modes;
			}
			else {
				modeflags |= modes;
			}
			reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HSET %s modeflags %d", chan_user_key.c_str(), modeflags);
			freeReplyObject(reply);

			SendUpdateUserChanModeflags(thread_data, request.channel_summary.channel_id, summary.id, modeflags, original_modeflags);

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
        
        response.error_details.response_code = WebErrorCode_Success;

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

		int cmd_count = 0;

		std::ostringstream mode_message;

		request.channel_summary = summary;

		int old_modeflags = summary.basic_mode_flags;

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


		if (request.channel_modify.unset_mode_flags != 0 || request.channel_modify.update_password || request.channel_modify.update_limit || request.channel_modify.unset_usermodes.size()) {
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

		std::string channel_key;
		std::ostringstream chan_ss;
		chan_ss << "channel_" << summary.channel_id;
		channel_key = chan_ss.str();

		
		redisAppendCommand(thread_data->mp_redis_connection, "HSET %s modeflags %d", channel_key.c_str(), summary.basic_mode_flags); cmd_count++;

		if (request.channel_modify.update_limit && request.channel_modify.limit != 0) {
			redisAppendCommand(thread_data->mp_redis_connection, "HSET %s limit %d", channel_key.c_str(), request.channel_modify.limit); cmd_count++;
		}
		else if(request.channel_modify.update_limit){
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s limit", channel_key.c_str()); cmd_count++;
		}
		if (request.channel_modify.update_password && request.channel_modify.password.length() != 0) {
			redisAppendCommand(thread_data->mp_redis_connection, "HSET %s password %s", channel_key.c_str(), request.channel_modify.password.c_str()); cmd_count++;
		}
		else if (request.channel_modify.update_password) {
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s password", channel_key.c_str()); cmd_count++;
		}

		if(request.channel_modify.update_topic && request.channel_modify.topic.length() != 0) {
			redisAppendCommand(thread_data->mp_redis_connection, "HSET %s topic %s", channel_key.c_str(), request.channel_modify.topic.c_str()); cmd_count++;
			uv_timespec64_t now;
			uv_clock_gettime(UV_CLOCK_REALTIME, &now);
			redisAppendCommand(thread_data->mp_redis_connection, "HSET %s topic_time %d", channel_key.c_str(), now.tv_sec); cmd_count++;
			redisAppendCommand(thread_data->mp_redis_connection, "HSET %s topic_user %s", channel_key.c_str(), request.peer->GetUserDetails().ToString().c_str()); cmd_count++;
		} else if(request.channel_modify.update_topic) {
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s topic", channel_key.c_str()); cmd_count++;
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s topic_time", channel_key.c_str()); cmd_count++;
			redisAppendCommand(thread_data->mp_redis_connection, "HDEL %s topic_user", channel_key.c_str()); cmd_count++;
		}

		for(int i = 0;i < cmd_count;i++) {
			void *reply;
        	redisGetReply(thread_data->mp_redis_connection,(void**)&reply);		
        	freeReplyObject(reply);
		}

		const char *base64 = OS::BinToBase64Str((uint8_t *)mode_message.str().c_str(), mode_message.str().length());
		std::string b64_string = base64;
		free((void *)base64);


		if (mode_message.str().size()) {
			std::ostringstream mq_message;
			mq_message << "\\type\\MODE\\toChannelId\\" << summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id << "\\includeSelf\\1" << "\\oldModeflags\\" << old_modeflags << "\\newModeflags\\" << summary.basic_mode_flags;

			sendAMQPMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
		}

		if (request.channel_modify.topic.size()) {
			const char* base64 = OS::BinToBase64Str((uint8_t*)request.channel_modify.topic.c_str(), request.channel_modify.topic.length());
			b64_string = base64;
			free((void*)base64);


			std::ostringstream mq_message;
			mq_message << "\\type\\TOPIC\\toChannelId\\" << summary.channel_id << "\\message\\" << b64_string << "\\fromUserId\\" << request.summary.id << "\\includeSelf\\1";

			sendAMQPMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
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
