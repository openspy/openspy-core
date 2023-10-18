#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>

namespace Peerchat {
	uv_once_t mm_tls_init_once = UV_ONCE_INIT;
	uv_key_t mm_redis_connection_key;
	uv_key_t mm_amqp_connection_key;

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

		// TaskScheduler<PeerchatBackendRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		// 	{EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails},
		// 	{EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget},
		// 	{EPeerchatRequestType_UserJoinChannel, Perform_UserJoinChannel},
		// 	{EPeerchatRequestType_UserPartChannel, Perform_UserPartChannel},
		// 	{EPeerchatRequestType_UserKickChannel, Perform_UserKickChannel},
		// 	{EPeerchatRequestType_LookupChannelDetails, Perform_LookupChannelDetails},
		// 	{EPeerchatRequestType_LookupUserDetailsByName, Perform_LookupUserDetailsByName},
		// 	{EPeerchatRequestType_UpdateChannelModes, Perform_UpdateChannelModes},
		// 	{EPeerchatRequestType_UpdateUserModes, Perform_UpdateUserModes},
		// 	{EPeerchatRequestType_ListChannels, Perform_ListChannels},
		// 	{EPeerchatRequestType_SetChannelUserKeys, Perform_SetChannelUserKeys},
		// 	{EPeerchatRequestType_GetChannelUserKeys, Perform_GetChannelUserKeys},
		// 	{EPeerchatRequestType_SetUserKeys, Perform_SetUserKeys},
		// 	{EPeerchatRequestType_GetUserKeys, Perform_GetUserKeys},
		// 	{EPeerchatRequestType_SetChannelKeys, Perform_SetChannelKeys},
		// 	{EPeerchatRequestType_GetChannelKeys, Perform_GetChannelKeys},

		// 	{EPeerchatRequestType_SetBroadcastToVisibleUsers, Perform_SetBroadcastToVisibleUsers},
		// 	{EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary, Perform_SetBroadcastToVisibleUsers},
		// 	{EPeerchatRequestType_SetBroadcastToVisibleUsers_SkipSource, Perform_SetBroadcastToVisibleUsers},

		// 	{EPeerchatRequestType_DeleteUser, Perform_DeleteUser},
		// 	{EPeerchatRequestType_KeepaliveUser, Perform_KeepaliveUser},
		// 	{EPeerchatRequestType_UserJoinEvents, Perform_UserJoinEvents},

		// 	{EPeerchatRequestType_CreateUserMode, Perform_SetUsermode},
		// 	{EPeerchatRequestType_ListUserModes, Perform_ListUsermodes},
		// 	{EPeerchatRequestType_ListUserModes_CacheLookup, Perform_ListUsermodes_Cached},
			
		// 	{EPeerchatRequestType_DeleteUserMode, Perform_DeleteUsermode},
		// 	{EPeerchatRequestType_LookupGameInfo, Perform_LookupGameInfo},
		// 	{EPeerchatRequestType_LookupGlobalUsermode, Perform_LookupGlobalUsermode},

		// 	{EPeerchatRequestType_SetChanProps, Perform_SetChanprops},
		// 	{EPeerchatRequestType_ListChanProps, Perform_ListChanprops},
		// 	{EPeerchatRequestType_DeleteChanProps, Perform_DelChanprops},

		// 	{EPeerchatRequestType_RemoteKill_ByName, Perform_RemoteKill_ByName},
		// 	{EPeerchatRequestType_UpdateChannelModes_BanMask, Perform_UpdateChannelModes_BanMask},
		// 	{EPeerchatRequestType_OperCheck, Perform_OperCheck},
		// 	{EPeerchatRequestType_CountServerUsers, Perform_CountServerUsers},
		// 	{-1, NULL}
		// };

		// TaskScheduler<PeerchatBackendRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		// 	{peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg},
		// 	{peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates},
		// 	{peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast},
		// 	{NULL, NULL, NULL}
		// };

        // TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
		// 	server_userSummary = new UserSummary();
		// 	server_userSummary->nick = "SERVER";
		// 	server_userSummary->username = "SERVER";
		// 	server_userSummary->hostname = "Matrix";
		// 	server_userSummary->id = -1;

		// 	channel_mode_flag_map = (ModeFlagMap*)&local_channel_mode_flag_map;
		// 	num_channel_mode_flags = sizeof(local_channel_mode_flag_map) / sizeof(ModeFlagMap);

		// 	user_mode_flag_map = (ModeFlagMap*)&local_user_mode_flag_map;
		// 	num_user_mode_flags = sizeof(local_user_mode_flag_map) / sizeof(ModeFlagMap);

		// 	user_join_chan_flag_map = (ModeFlagMap*)&local_user_join_chan_flag_map;
		// 	num_user_join_chan_flags = sizeof(local_user_join_chan_flag_map) / sizeof(ModeFlagMap);

        //     TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

			
		// 	scheduler->DeclareReady();

        //     return scheduler;
        // }


	void do_init_thread_tls_key() {
		uv_key_create(&mm_redis_connection_key);
		uv_key_create(&mm_amqp_connection_key);
	}

		bool Handle_Broadcast(TaskThreadData *thread_data, std::string message) {
			Peerchat::Server* server = (Peerchat::Server*)uv_loop_get_data(uv_default_loop());

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
			Peerchat::Server* server = (Peerchat::Server*)uv_loop_get_data(uv_default_loop());
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
				ss.str("");
			}
			ss << "HMSET " << base_key;

			if(user_base.length() > 0) {
				ss << " " << user_base << "username" << " " << userSummary.username; 
				ss << " " << user_base << "nick" << " " << userSummary.nick;
				ss << " " << user_base << "realname" << " " << userSummary.realname;
				ss << " " << user_base << "gameid" << " " << userSummary.gameid; 
				ss << " " << user_base << "hostname" << " " << userSummary.hostname; 
				ss << " " << user_base << "profileid" << " " << userSummary.profileid; 
				ss << " " << user_base << "userid" << " " << userSummary.userid; 
				ss << " " << user_base << "backendid" << " " << userSummary.id;

				if(show_private) {
					ss << " " << user_base << "address" << " " << userSummary.address.ToString(true);
					ss << " " << user_base << "modeflags" << " " << userSummary.modeflags;
					ss << " " << user_base << "operflags" << " " << userSummary.operflags;
				}
			} else {
				ss << " " << "username" << " " << userSummary.username; 
				ss << " " << "nick" << " " << userSummary.nick;
				ss << " " << "realname" << " " << userSummary.realname;
				ss << " " << "gameid" << " " << userSummary.gameid; 
				ss << " " << "hostname" << " " << userSummary.hostname; 
				ss << " " << "profileid" << " " << userSummary.profileid; 
				ss << " " << "userid" << " " << userSummary.userid; 
				ss << " " << "backendid" << " " << userSummary.id;

				if(show_private) {
					ss << " " << "address" << " " << userSummary.address.ToString(true);
					ss << " " << "modeflags" << " " << userSummary.modeflags;
					ss << " " << "operflags" << " " << userSummary.operflags;
				}
			}

			void *reply =redisCommand(thread_data->mp_redis_connection, ss.str().c_str());
			freeReplyObject(reply);
			
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
		void AddPeerchatTaskRequest(PeerchatBackendRequest request) {

		}

	amqp_connection_state_t getThreadLocalAmqpConnection() {
		uv_once(&mm_tls_init_once, do_init_thread_tls_key);
		amqp_connection_state_t connection = (amqp_connection_state_t)uv_key_get(&mm_amqp_connection_key);

		if(connection != NULL) {
			return connection;
		}

		char address_buffer[32];
		char port_buffer[32];

		char user_buffer[32];
		char pass_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);

		connection = amqp_new_connection();
		amqp_socket_t *amqp_socket = amqp_tcp_socket_new(connection);


		uv_os_getenv("OPENSPY_AMQP_ADDRESS", (char *)&address_buffer, &temp_env_sz);

		temp_env_sz = sizeof(port_buffer);
		uv_os_getenv("OPENSPY_AMQP_PORT", (char *)&port_buffer, &temp_env_sz);
		int status = amqp_socket_open(amqp_socket, address_buffer, atoi(port_buffer));

		temp_env_sz = sizeof(user_buffer);
		uv_os_getenv("OPENSPY_AMQP_USER", (char *)&user_buffer, &temp_env_sz);
		temp_env_sz = sizeof(pass_buffer);
		uv_os_getenv("OPENSPY_AMQP_PASSWORD", (char *)&pass_buffer, &temp_env_sz);

		if(status) {
			perror("error opening amqp listener socket");
		}

		amqp_login(connection, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);

		amqp_channel_open(connection, 1);

		uv_key_set(&mm_amqp_connection_key, connection);

		return connection;

	}

	void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody) {
		amqp_connection_state_t connection = getThreadLocalAmqpConnection();

		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = 1;

		amqp_basic_publish(connection, 1, amqp_cstring_bytes(exchange),
                                    amqp_cstring_bytes(routingkey), 0, 0,
                                    &props, amqp_cstring_bytes(messagebody));

	}
}