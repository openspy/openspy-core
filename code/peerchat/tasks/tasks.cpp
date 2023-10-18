#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

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


	// TaskScheduler<PeerchatBackendRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
	// 	{peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg},
	// 	{peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates},
	// 	{peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast},
	// 	{NULL, NULL, NULL}
	// };


	typedef struct {
		const char *amqp_exchange;
		const char *amqp_routing_key;
		bool (*amqp_event_callback)(TaskThreadData *, std::string);
		uv_thread_t amqp_authevent_consumer_thread;
		amqp_connection_state_t amqp_listener_conn;
		amqp_socket_t *amqp_socket;
	} ListenerArgs;

	ListenerArgs consume_privmsg_message = {peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg};
	ListenerArgs consume_keyupdates_message = {peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates};
	ListenerArgs consume_broadcast_message = {peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast};



	void amqp_do_consume(void *arg) {
		ListenerArgs *listener_args = (ListenerArgs *)arg;
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;		
		
		amqp_bytes_t queuename;

		amqp_queue_declare_ok_t *r = amqp_queue_declare(
        	listener_args->amqp_listener_conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);

		queuename = amqp_bytes_malloc_dup(r->queue);

  		amqp_queue_bind(listener_args->amqp_listener_conn, 1, queuename, amqp_cstring_bytes(listener_args->amqp_exchange),
                  amqp_cstring_bytes(listener_args->amqp_routing_key), amqp_empty_table);

		for(;;) {
			res = amqp_consume_message(listener_args->amqp_listener_conn, &envelope, NULL, 0);

			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}

			std::string message = std::string((const char *)envelope.message.body.bytes, envelope.message.body.len);

			listener_args->amqp_event_callback(NULL, message);

			amqp_destroy_envelope(&envelope);
		}

		amqp_channel_close(listener_args->amqp_listener_conn, 1, AMQP_REPLY_SUCCESS);
		amqp_connection_close(listener_args->amqp_listener_conn, AMQP_REPLY_SUCCESS);
		amqp_destroy_connection(listener_args->amqp_listener_conn);
	}

	void InitTasks() {
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

		char address_buffer[32];
		char port_buffer[32];

		char user_buffer[32];
		char pass_buffer[32];
		size_t temp_env_sz = sizeof(address_buffer);

		uv_os_getenv("OPENSPY_AMQP_ADDRESS", (char *)&address_buffer, &temp_env_sz);

		temp_env_sz = sizeof(port_buffer);
		uv_os_getenv("OPENSPY_AMQP_PORT", (char *)&port_buffer, &temp_env_sz);		

		temp_env_sz = sizeof(user_buffer);
		uv_os_getenv("OPENSPY_AMQP_USER", (char *)&user_buffer, &temp_env_sz);
		temp_env_sz = sizeof(pass_buffer);
		uv_os_getenv("OPENSPY_AMQP_PASSWORD", (char *)&pass_buffer, &temp_env_sz);



		//setup privmsg listener
		consume_privmsg_message.amqp_listener_conn = amqp_new_connection();
		consume_privmsg_message.amqp_socket = amqp_tcp_socket_new(consume_privmsg_message.amqp_listener_conn);
		int status = amqp_socket_open(consume_privmsg_message.amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			perror("error opening privmsg amqp listener socket");
		}
		amqp_login(consume_privmsg_message.amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(consume_privmsg_message.amqp_listener_conn, 1);
		uv_thread_create(&consume_privmsg_message.amqp_authevent_consumer_thread, amqp_do_consume, &consume_privmsg_message);

		//setup keyupdates listener
		consume_keyupdates_message.amqp_listener_conn = amqp_new_connection();
		consume_keyupdates_message.amqp_socket = amqp_tcp_socket_new(consume_keyupdates_message.amqp_listener_conn);
		status = amqp_socket_open(consume_keyupdates_message.amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			perror("error opening keyupdates amqp listener socket");
		}
		amqp_login(consume_keyupdates_message.amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(consume_keyupdates_message.amqp_listener_conn, 1);
		uv_thread_create(&consume_keyupdates_message.amqp_authevent_consumer_thread, amqp_do_consume, &consume_keyupdates_message);

		//setup keyupdates listener
		consume_broadcast_message.amqp_listener_conn = amqp_new_connection();
		consume_broadcast_message.amqp_socket = amqp_tcp_socket_new(consume_broadcast_message.amqp_listener_conn);
		status = amqp_socket_open(consume_broadcast_message.amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			perror("error opening broadcast amqp listener socket");
		}
		amqp_login(consume_broadcast_message.amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(consume_broadcast_message.amqp_listener_conn, 1);
		uv_thread_create(&consume_broadcast_message.amqp_authevent_consumer_thread, amqp_do_consume, &consume_broadcast_message);
		
	}


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

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		PeerchatBackendRequest *work_data = (PeerchatBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->type) {
			case EPeerchatRequestType_SetUserDetails:
				Perform_SetUserDetails(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SendMessageToTarget:
				Perform_SendMessageToTarget(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UserJoinChannel:
				Perform_UserJoinChannel(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UserPartChannel:
				Perform_UserPartChannel(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UserKickChannel:
				Perform_UserKickChannel(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_LookupChannelDetails:
				Perform_LookupChannelDetails(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_LookupUserDetailsByName:
				Perform_LookupUserDetailsByName(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UpdateChannelModes:
				Perform_UpdateChannelModes(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UpdateUserModes:
				Perform_UpdateUserModes(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_ListChannels:
				Perform_ListChannels(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SetChannelUserKeys:
				Perform_SetChannelUserKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_GetChannelUserKeys:
				Perform_GetChannelUserKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SetUserKeys:
				Perform_SetUserKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_GetUserKeys:
				Perform_GetUserKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SetChannelKeys:
				Perform_SetChannelKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_GetChannelKeys:
				Perform_GetChannelKeys(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SetBroadcastToVisibleUsers:
			case EPeerchatRequestType_SetBroadcastToVisibleUsers_SendSummary:
			case EPeerchatRequestType_SetBroadcastToVisibleUsers_SkipSource:
				Perform_SetBroadcastToVisibleUsers(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_DeleteUser:
				Perform_DeleteUser(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_KeepaliveUser:
				Perform_KeepaliveUser(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UserJoinEvents:
				Perform_UserJoinEvents(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_CreateUserMode:
				Perform_SetUsermode(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_ListUserModes:
				Perform_ListUsermodes(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_ListUserModes_CacheLookup:
				Perform_ListUsermodes_Cached(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_DeleteUserMode:
				Perform_DeleteUsermode(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_LookupGameInfo:
				Perform_LookupGameInfo(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_LookupGlobalUsermode:
				Perform_LookupGlobalUsermode(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_SetChanProps:
				Perform_SetChanprops(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_ListChanProps:
				Perform_ListChanprops(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_DeleteChanProps:
				Perform_DelChanprops(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_RemoteKill_ByName:
				Perform_RemoteKill_ByName(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_UpdateChannelModes_BanMask:
				Perform_UpdateChannelModes_BanMask(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_OperCheck:
				Perform_OperCheck(*work_data, &temp_data);
			break;
			case EPeerchatRequestType_CountServerUsers:
				Perform_CountServerUsers(*work_data, &temp_data);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		PeerchatBackendRequest *work_data = (PeerchatBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}

	void AddPeerchatTaskRequest(PeerchatBackendRequest request) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		PeerchatBackendRequest *work_data = new PeerchatBackendRequest();
		*work_data = request;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);
		uv_queue_work(uv_default_loop(), uv_req, PerformUVWorkRequest, PerformUVWorkRequestCleanup);
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