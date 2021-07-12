#include "tasks.h"
#include <sstream>
#include <server/Server.h>
namespace Peerchat {
        const char *peerchat_channel_exchange = "peerchat.core";

		/*
			this queue is used for PRIVMSG,NOTICE,ATM,UTM,MODE,TOPIC, JOIN, PART
		*/
        const char *peerchat_client_message_routingkey = "peerchat.client-messages";
		const char* peerchat_key_updates_routingkey = "peerchat.keyupdate-messages";
		const char* peerchat_broadcast_routingkey = "peerchat.client-broadcasts";

		UserSummary* server_userSummary = NULL;

		ModeFlagMap* channel_mode_flag_map = NULL;
		int num_channel_mode_flags = 0;
		ModeFlagMap local_channel_mode_flag_map[] = {
			{EChannelMode_NoOutsideMessages, 'n'},
			{EChannelMode_TopicProtect, 't'},
			{EChannelMode_Moderated, 'm'},
			{EChannelMode_Private, 'p'},
			{EChannelMode_Secret, 's'},
			{EChannelMode_InviteOnly, 'i'},
			{EChannelMode_StayOpen, 'z'},
			{EChannelMode_UserCreated, 'r'},
			{EChannelMode_OpsObeyChannelLimit, 'e'},
			{EChannelMode_Auditorium, 'u'},
			{EChannelMode_Auditorium_ShowVOP, 'q'}
		};

		ModeFlagMap* user_mode_flag_map = NULL;
		int num_user_mode_flags = 0;
		ModeFlagMap local_user_mode_flag_map[] = {
			{EUserMode_Quiet, 'q'},
			{EUserMode_Invisible, 'i'},
			{EUserMode_Gagged, 'g'}
		};

		ModeFlagMap* user_join_chan_flag_map = NULL;
		int num_user_join_chan_flags = 0;
		ModeFlagMap local_user_join_chan_flag_map[] = {
			{EUserChannelFlag_Quiet, 'q'},
			{EUserChannelFlag_Invisible, 'i'},
			{EUserChannelFlag_Gagged, 'g'},
			{EUserChannelFlag_Invited, 'I'},
			{EUserChannelFlag_Voice, 'v'},
			{EUserChannelFlag_HalfOp, 'h'},
			{EUserChannelFlag_Op, 'o'},
			{EUserChannelFlag_Owner, 'O'},
			{EUserChannelFlag_Banned, 'b'},
			{EUserChannelFlag_GameidPermitted, 'p'}
		};

		TaskScheduler<PeerchatBackendRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
			{EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails},
			{EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget},
			{EPeerchatRequestType_UserJoinChannel, Perform_UserJoinChannel},
			{EPeerchatRequestType_UserPartChannel, Perform_UserPartChannel},
			{EPeerchatRequestType_UserKickChannel, Perform_UserKickChannel},
			{EPeerchatRequestType_LookupChannelDetails, Perform_LookupChannelDetails},
			{EPeerchatRequestType_LookupUserDetailsByName, Perform_LookupUserDetailsByName},
			{EPeerchatRequestType_UpdateChannelModes, Perform_UpdateChannelModes},
			{EPeerchatRequestType_UpdateUserModes, Perform_UpdateUserModes},
			{EPeerchatRequestType_ListChannels, Perform_ListChannels},
			{EPeerchatRequestType_SetChannelUserKeys, Perform_SetChannelUserKeys},
			{EPeerchatRequestType_GetChannelUserKeys, Perform_GetChannelUserKeys},
			{EPeerchatRequestType_SetUserKeys, Perform_SetUserKeys},
			{EPeerchatRequestType_GetUserKeys, Perform_GetUserKeys},
			{EPeerchatRequestType_SetChannelKeys, Perform_SetChannelKeys},
			{EPeerchatRequestType_GetChannelKeys, Perform_GetChannelKeys},

			{EPeerchatRequestType_SetBroadcastToVisibleUsers, Perform_SetBroadcastToVisibleUsers},
			{EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary, Perform_SetBroadcastToVisibleUsers},
			{EPeerchatRequestType_SetBroadcastToVisibleUsers_SkipSource, Perform_SetBroadcastToVisibleUsers},

			{EPeerchatRequestType_DeleteUser, Perform_DeleteUser},
			{EPeerchatRequestType_KeepaliveUser, Perform_KeepaliveUser},
			{EPeerchatRequestType_UserJoinEvents, Perform_UserJoinEvents},

			{EPeerchatRequestType_CreateUserMode, Perform_SetUsermode},
			{EPeerchatRequestType_ListUserModes, Perform_ListUsermodes},
			{EPeerchatRequestType_ListUserModes_CacheLookup, Perform_ListUsermodes_Cached},
			
			{EPeerchatRequestType_DeleteUserMode, Perform_DeleteUsermode},
			{EPeerchatRequestType_LookupGameInfo, Perform_LookupGameInfo},
			{EPeerchatRequestType_LookupGlobalUsermode, Perform_LookupGlobalUsermode},

			{EPeerchatRequestType_SetChanProps, Perform_SetChanprops},
			{EPeerchatRequestType_ListChanProps, Perform_ListChanprops},
			{EPeerchatRequestType_DeleteChanProps, Perform_DelChanprops},

			{EPeerchatRequestType_RemoteKill_ByName, Perform_RemoteKill_ByName},
			{EPeerchatRequestType_UpdateChannelModes_BanMask, Perform_UpdateChannelModes_BanMask},
			{EPeerchatRequestType_OperCheck, Perform_OperCheck},
			{NULL, NULL}
		};

		TaskScheduler<PeerchatBackendRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
			{peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg},
			{peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates},
			{peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast},
			{NULL, NULL, NULL}
		};

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
			server_userSummary = new UserSummary();
			server_userSummary->nick = "SERVER";
			server_userSummary->username = "SERVER";
			server_userSummary->hostname = "Matrix";
			server_userSummary->id = -1;

			channel_mode_flag_map = (ModeFlagMap*)&local_channel_mode_flag_map;
			num_channel_mode_flags = sizeof(local_channel_mode_flag_map) / sizeof(ModeFlagMap);

			user_mode_flag_map = (ModeFlagMap*)&local_user_mode_flag_map;
			num_user_mode_flags = sizeof(local_user_mode_flag_map) / sizeof(ModeFlagMap);

			user_join_chan_flag_map = (ModeFlagMap*)&local_user_join_chan_flag_map;
			num_user_join_chan_flags = sizeof(local_user_join_chan_flag_map) / sizeof(ModeFlagMap);

            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

			
			scheduler->DeclareReady();

            return scheduler;
        }


		bool Handle_Broadcast(TaskThreadData *thread_data, std::string message) {
			Peerchat::Server* server = (Peerchat::Server*)thread_data->server;

			OS::KVReader reader = message;
			
			uint8_t* data_out;
			size_t data_len;
			OS::Base64StrToBin((const char*)reader.GetValue("message").c_str(), &data_out, data_len);
			std::string send_message = std::string((const char*)data_out, data_len);
			free((void*)data_out);

			std::string type = reader.GetValue("type");

			UserSummary from;
			if (reader.HasKey("fromUserId")) {
				from = LookupUserById(thread_data, reader.GetValueInt("fromUserId"));
			} else if(reader.HasKey("fromSummary")) {
				from = UserSummary::FromBase64String(reader.GetValue("fromSummary"));
			}

			std::string channels = reader.GetValue("channels");
			std::string channel_modes = reader.GetValue("channel_modes");

			bool includeSelf = reader.GetValueInt("includeSelf");

			std::vector<int> channel_list;
			std::istringstream input;
			input.str(channels);
			std::string s;
			while (std::getline(input, s, ',')) {
				channel_list.push_back(atoi(s.c_str()));
			}

			std::vector<int> channel_modes_list;
			input = std::istringstream(channel_modes);
			while (std::getline(input, s, ',')) {
				channel_modes_list.push_back(atoi(s.c_str()));
			}

			std::map<int, int> channel_mode_map;

			int num_items = channel_list.size();
			for (int i = 0; i < num_items; i++) {
				channel_mode_map[channel_list.at(i)] = channel_modes_list.at(i);
			}

			server->OnChannelBroadcast(type, from, channel_mode_map, send_message, includeSelf);

			return true;
		}

		bool Handle_KeyUpdates(TaskThreadData *thread_data, std::string message) {
			Peerchat::Server* server = (Peerchat::Server*)thread_data->server;
			OS::KVReader reader = message;
			
			uint8_t* data_out;
			size_t data_len;
			OS::Base64StrToBin((const char*)reader.GetValue("keys").c_str(), &data_out, data_len);
			std::string send_message = std::string((const char*)data_out, data_len);
			free((void*)data_out);
			OS::KVReader keys = send_message;
			
			ChannelSummary summary;
				
			UserSummary user_summary;
			int mode_flags;

			if(reader.HasKey("channel_id")) {
				summary = LookupChannelById(thread_data, reader.GetValueInt("channel_id"));
			}

			if(reader.HasKey("user_id")) {
				user_summary = LookupUserById(thread_data, reader.GetValueInt("user_id"));
			}
			
			if (reader.GetValue("type").compare("SETCKEY") == 0) {
				server->OnSetUserChannelKeys(summary, user_summary, keys);
			} else if (reader.GetValue("type").compare("SETCHANKEY") == 0) {
				server->OnSetChannelKeys(summary, keys);
			} else if (reader.GetValue("type").compare("UPDATE_USER_CHANMODEFLAGS") == 0) {
				mode_flags = reader.GetValueInt("modeflags");
				server->OnUpdateChanUsermode(summary.channel_id, user_summary, mode_flags);
			} else if (reader.GetValue("type").compare("REMOTE_KILL") == 0) {
				
				OS::Base64StrToBin((const char*)reader.GetValue("reason").c_str(), &data_out, data_len);
				std::string kill_reason = (const char *)data_out;
				free((void *)data_out);
				server->OnKillUser(user_summary, kill_reason);
			}
			return false;
		}
		

		void ApplyUserKeys(TaskThreadData* thread_data, std::string base_key, UserSummary userSummary, std::string user_base, bool show_private) {
			std::ostringstream ss;
			if(base_key.length() == 0) {
				ss << "user_" << userSummary.id;
				base_key = ss.str();
			}
			
			if(user_base.length() > 0) {
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %susername %s", base_key.c_str(), user_base.c_str(), userSummary.username.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %snick %s", base_key.c_str(), user_base.c_str(), userSummary.nick.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %srealname %s", base_key.c_str(), user_base.c_str(), userSummary.realname.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %sgameid %d", base_key.c_str(), user_base.c_str(), userSummary.gameid);
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %shostname %s", base_key.c_str(), user_base.c_str(), userSummary.hostname.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %sprofileid %d", base_key.c_str(), user_base.c_str(), userSummary.profileid);
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %suserid %d", base_key.c_str(), user_base.c_str(), userSummary.userid);
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %sbackendid %d", base_key.c_str(), user_base.c_str(), userSummary.id);

				if(show_private) {
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %saddress %s", base_key.c_str(), user_base.c_str(), userSummary.address.ToString(true).c_str());
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %smodeflags %d", base_key.c_str(), user_base.c_str(), userSummary.modeflags);
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s %operflags %d", base_key.c_str(), user_base.c_str(), userSummary.operflags);
				}
			} else {
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s username %s", base_key.c_str(), userSummary.username.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s nick %s", base_key.c_str(), userSummary.nick.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s realname %s", base_key.c_str(), userSummary.realname.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", base_key.c_str(), userSummary.gameid);
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s hostname %s", base_key.c_str(), userSummary.hostname.c_str());
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s profileid %d", base_key.c_str(), userSummary.profileid);
				Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s userid %d", base_key.c_str(), userSummary.userid);

				if(show_private) {
					
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s address %s", base_key.c_str(), userSummary.address.ToString(true).c_str());
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s modeflags %d", base_key.c_str(), userSummary.modeflags);
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s operflags %d", base_key.c_str(), userSummary.operflags);
				}
			}
		}

		int channelUserModesStringToFlags(std::string mode_string) {
			int flags = 0;
			for (int i = 0; i < num_user_join_chan_flags; i++) {
				if (mode_string.find(user_join_chan_flag_map[i].character) != std::string::npos) {
					flags |= user_join_chan_flag_map[i].flag;
				}
			}
			return flags;
		}
		std::string modeFlagsToModeString(int modeflags) {
			std::ostringstream ss;
			for (int i = 0; i < num_user_join_chan_flags; i++) {
				if (modeflags & user_join_chan_flag_map[i].flag) {
					ss << user_join_chan_flag_map[i].character;
				}
			}
			return ss.str();
		}
		json_t* GetJsonFromUserSummary(UserSummary summary) {
			json_t* item = json_object();
			json_object_set_new(item, "id", json_integer(summary.id));
			json_object_set_new(item, "nick", json_string(summary.nick.c_str()));
			json_object_set_new(item, "username", json_string(summary.username.c_str()));
			json_object_set_new(item, "realname", json_string(summary.realname.c_str()));
			json_object_set_new(item, "hostname", json_string(summary.hostname.c_str()));
			json_object_set_new(item, "address", json_string(summary.address.ToString().c_str()));

			json_t *gameid = json_null();
			if(summary.gameid != -1) {
				gameid = json_integer(summary.gameid);
			}

			json_object_set_new(item, "gameid", gameid);
			
			json_object_set_new(item, "profileid", json_integer(summary.profileid));
			return item;
		}
}