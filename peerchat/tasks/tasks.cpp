#include "tasks.h"
#include <sstream>
#include <server/Server.h>
namespace Peerchat {
        const char *peerchat_channel_exchange = "openspy.core";

		/*
			this queue is used for PRIVMSG,NOTICE,ATM,UTM,MODE,TOPIC, JOIN, PART
		*/
        const char *peerchat_client_message_routingkey = "peerchat.client-messages";
		const char* peerchat_key_updates_routingkey = "peerchat.keyupdate-messages";
		const char* peerchat_broadcast_routingkey = "peerchat.client-broadcasts";

		ModeFlagMap* channel_mode_flag_map = NULL;
		int num_channel_mode_flags;
		ModeFlagMap local_channel_mode_flag_map[] = {
			{EChannelMode_NoOutsideMessages, 'n'},
			{EChannelMode_TopicProtect, 't'},
			{EChannelMode_Moderated, 'm'},
			{EChannelMode_Private, 'p'},
			{EChannelMode_Secret, 's'},
			{EChannelMode_InviteOnly, 'i'},
			{EChannelMode_StayOpen, 'z'},
			{EChannelMode_Registered, 'r'},
			{EChannelMode_OpsObeyChannelLimit, 'e'},
			{EChannelMode_Auditorium, 'u'},
			{EChannelMode_AuditoriumVOP, 'q'}
		};

		ModeFlagMap* user_mode_flag_map = NULL;
		int num_user_mode_flags;
		ModeFlagMap local_user_mode_flag_map[] = {
			{EUserMode_Quiet, 'q'},
			{EUserMode_Invisible, 'i'},
		};
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
			channel_mode_flag_map = (ModeFlagMap*)&local_channel_mode_flag_map;
			num_channel_mode_flags = sizeof(local_channel_mode_flag_map) / sizeof(ModeFlagMap);

			user_mode_flag_map = (ModeFlagMap*)&local_user_mode_flag_map;
			num_user_mode_flags = sizeof(local_user_mode_flag_map) / sizeof(ModeFlagMap);

            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server);
            scheduler->AddRequestHandler(EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails);
            scheduler->AddRequestHandler(EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget);
            scheduler->AddRequestHandler(EPeerchatRequestType_UserJoinChannel, Perform_UserJoinChannel);
			scheduler->AddRequestHandler(EPeerchatRequestType_UserPartChannel, Perform_UserPartChannel);
			scheduler->AddRequestHandler(EPeerchatRequestType_UserKickChannel, Perform_UserKickChannel);			
			scheduler->AddRequestHandler(EPeerchatRequestType_LookupChannelDetails, Perform_LookupChannelDetails);
			scheduler->AddRequestHandler(EPeerchatRequestType_LookupUserDetailsByName, Perform_LookupUserDetailsByName);
			scheduler->AddRequestHandler(EPeerchatRequestType_UpdateChannelModes, Perform_UpdateChannelModes);
			scheduler->AddRequestHandler(EPeerchatRequestType_UpdateUserModes, Perform_UpdateUserModes);
			scheduler->AddRequestHandler(EPeerchatRequestType_ListChannels, Perform_ListChannels);
			scheduler->AddRequestHandler(EPeerchatRequestType_SetChannelUserKeys, Perform_SetChannelUserKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_GetChannelUserKeys, Perform_GetChannelUserKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_SetUserKeys, Perform_SetUserKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_GetUserKeys, Perform_GetUserKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_SetChannelKeys, Perform_SetChannelKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_GetChannelKeys, Perform_GetChannelKeys);

			scheduler->AddRequestHandler(EPeerchatRequestType_SetBroadcastToVisibleUsers, Perform_SetBroadcastToVisibleUsers);
			scheduler->AddRequestHandler(EPeerchatRequestType_SetBroadcastToVisibleUsers_SkipSource, Perform_SetBroadcastToVisibleUsers);

			scheduler->AddRequestHandler(EPeerchatRequestType_DeleteUser, Perform_DeleteUser);
			scheduler->AddRequestHandler(EPeerchatRequestType_KeepaliveUser, Perform_KeepaliveUser);

			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_PrivMsg);
			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_key_updates_routingkey, Handle_KeyUpdates);
			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_broadcast_routingkey, Handle_Broadcast);

			
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
			std::string target = reader.GetValue("source");

			std::string channels = reader.GetValue("channels");

			bool includeSelf = reader.GetValueInt("includeSelf");

			std::vector<int> channel_list;

			std::istringstream input;
			input.str(channels);
			std::string s;
			while(std::getline(input, s, ',')) {
				channel_list.push_back(atoi(s.c_str()));
			}

			server->OnChannelBroadcast(type, target, channel_list, send_message, includeSelf);

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
			ChannelSummary summary = GetChannelSummaryByName(thread_data, reader.GetValue("to"), false);
			UserSummary user_summary;
			if(reader.HasKey("user_id")) {
				user_summary = LookupUserById(thread_data, reader.GetValueInt("user_id"));
			}
			
			if (reader.GetValue("type").compare("SETCKEY") == 0) {
				server->OnSetUserChannelKeys(summary, user_summary, keys);
			} else if (reader.GetValue("type").compare("SETCHANKEY") == 0) {
				server->OnSetChannelKeys(summary, keys);
			}
			return false;
		}
}