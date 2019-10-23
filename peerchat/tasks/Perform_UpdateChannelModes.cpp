#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

    bool Perform_UpdateChannelModes(PeerchatBackendRequest request, TaskThreadData *thread_data) {

		std::string target;
		if (request.channel_summary.channel_name.length() > 0) {
			target = request.channel_summary.channel_name;
		}

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);

		TaskResponse response;

		std::ostringstream mode_message;

		if (request.channel_modify.set_mode_flags != 0 || request.channel_modify.update_password || request.channel_modify.update_limit) {
			bool added_mode = false;
			for (int i = 0; i < num_mode_flags; i++) {
				if (request.channel_modify.set_mode_flags & mode_flag_map[i].flag) {
					if (~summary.basic_mode_flags & mode_flag_map[i].flag) {
						if (added_mode == false) {
							mode_message << "+";
							added_mode = true;
						}
						mode_message << mode_flag_map[i].character;
						summary.basic_mode_flags |= mode_flag_map[i].flag;
						
					}
				}
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
		}


		if (request.channel_modify.unset_mode_flags != 0 || request.channel_modify.update_password || request.channel_modify.update_limit) {
			bool removed_mode = false;
			for (int i = 0; i < num_mode_flags; i++) {
				if (request.channel_modify.unset_mode_flags & mode_flag_map[i].flag) {
					if (summary.basic_mode_flags & mode_flag_map[i].flag) {
						if (removed_mode == false) {
							mode_message << "-";
							removed_mode = true;
						}
						mode_message << mode_flag_map[i].character;
						summary.basic_mode_flags &= ~mode_flag_map[i].flag;
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

			if (request.channel_modify.update_password && request.channel_modify.password.length() != 0) {
				mode_message << " " << request.channel_modify.password;
			}

			if (request.channel_modify.update_limit && request.channel_modify.limit != 0) {
				mode_message << " " << request.channel_modify.limit;
			}
		}

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat); 
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
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET channel_%d topic_time %d", now.tv_sec);
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
			mq_message << "\\type\\MODE\\to\\" << target << "\\message\\" << b64_string << "\\from\\" << request.summary.ToString();

			thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, mq_message.str().c_str());
		}

		if (request.channel_modify.topic.size()) {
			request.channel_modify.topic = ":" + request.channel_modify.topic;
			const char* base64 = OS::BinToBase64Str((uint8_t*)request.channel_modify.topic.c_str(), request.channel_modify.topic.length());
			b64_string = base64;
			free((void*)base64);


			std::ostringstream mq_message;
			mq_message << "\\type\\TOPIC\\to\\" << target << "\\message\\" << b64_string << "\\from\\" << request.summary.ToString();

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