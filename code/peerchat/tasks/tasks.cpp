#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <server/Peer.h>


#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

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

	ListenerEventHandler consume_privmsg_handler = {peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg};
	ListenerEventHandler consume_keyupdates_handler = {peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates};
	ListenerEventHandler consume_broadcast_handler = {peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast};

	TaskShared::ListenerEventHandler all_events[] = {consume_privmsg_handler, consume_keyupdates_handler, consume_broadcast_handler};
	TaskShared::ListenerArgs peerchat_event_listener = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};


	void InitTasks() {
		Peerchat::Server* server = (Peerchat::Server*)uv_loop_get_data(uv_default_loop());
		std::string server_name = server->getServerName();

		server_userSummary = new UserSummary();
		server_userSummary->nick = "SERVER";
		server_userSummary->username = "SERVER";
		server_userSummary->hostname = server_name;
		server_userSummary->id = -1;

		channel_mode_flag_map = (ModeFlagMap*)&local_channel_mode_flag_map;
		num_channel_mode_flags = sizeof(local_channel_mode_flag_map) / sizeof(ModeFlagMap);

		user_mode_flag_map = (ModeFlagMap*)&local_user_mode_flag_map;
		num_user_mode_flags = sizeof(local_user_mode_flag_map) / sizeof(ModeFlagMap);

		user_join_chan_flag_map = (ModeFlagMap*)&local_user_join_chan_flag_map;
		num_user_join_chan_flags = sizeof(local_user_join_chan_flag_map) / sizeof(ModeFlagMap);

		setup_listener(&peerchat_event_listener);		
	}

	bool Handle_Broadcast(TaskThreadData *thread_data, std::string message) {
		Peerchat::Server* server = (Peerchat::Server*)uv_loop_get_data(uv_default_loop());

		OS::KVReader reader = message;
        
        redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
        freeReplyObject(reply);
		
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

		size_t num_items = channel_list.size();
		for (int i = 0; i < num_items; i++) {
			channel_mode_map[channel_list.at(i)] = channel_modes_list.at(i);
		}

		server->OnChannelBroadcast(type, from, channel_mode_map, send_message, includeSelf);

		return true;
	}

	bool Handle_KeyUpdates(TaskThreadData *thread_data, std::string message) {
		Peerchat::Server* server = (Peerchat::Server*)uv_loop_get_data(uv_default_loop());
		OS::KVReader reader = message;
        
        redisReply *reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
        freeReplyObject(reply);
		
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
		if(base_key.empty()) {
			ss << "user_" << userSummary.id;
			base_key = ss.str();
		}

		std::string key = base_key + user_base;

		std::string profileid, userid, backendid, address = userSummary.address.ToString(true), modeflags, operflags, gameid;

		ss.str("");
		ss << userSummary.gameid;
		gameid = ss.str();

		ss.str("");
		ss << userSummary.id;
		backendid = ss.str();

		ss.str("");
		ss << userSummary.profileid;
		profileid = ss.str();
		
		ss.str("");
		ss << userSummary.userid;
		userid = ss.str();

		ss.str("");
		ss << userSummary.modeflags;
		modeflags = ss.str();

		ss.str("");
		ss << userSummary.operflags;
		operflags = ss.str();

		const char *args[] = {
			"HMSET", key.c_str(), 
			"username", userSummary.username.c_str(),
			"nick", userSummary.nick.c_str(),
			"realname", userSummary.realname.c_str(),
			"gameid", gameid.c_str(),
			"hostname", userSummary.hostname.c_str(),
			"profileid", profileid.c_str(),
			"userid", userid.c_str(),
			"backendid", backendid.c_str(),

			//private args
			"address", address.c_str(),
			"modeflags", modeflags.c_str(),
			"operflags", operflags.c_str()
		};

		int num_args = sizeof(args) / sizeof(const char *);
		if(!show_private) {
			num_args -= 6; //don't submit private args
		}

        
		redisAppendCommandArgv(thread_data->mp_redis_connection, num_args, args, NULL);
		redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", key.c_str(), CHANNEL_EXPIRE_TIME);


		redisReply *reply;
		if(redisGetReply(thread_data->mp_redis_connection,(void**)&reply) == REDIS_OK) {
			freeReplyObject(reply);
		}		
		
		if(redisGetReply(thread_data->mp_redis_connection,(void**)&reply) == REDIS_OK) {
			freeReplyObject(reply);
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

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		PeerchatBackendRequest *work_data = (PeerchatBackendRequest *) uv_handle_get_data((uv_handle_t*) req);

		if (work_data->peer) {
			work_data->channel_flags = work_data->peer->GetChannelFlagsMap();
		}
        
        redisReply *reply = (redisReply *)redisCommand(temp_data.mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
        freeReplyObject(reply);
		
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
}
