#include "tasks.h"
namespace Peerchat {
        const char *peerchat_channel_exchange = "openspy.core";
        const char *peerchat_client_message_routingkey = "peerchat.client-messages";
        const char *peerchat_channel_message_routingkey = "peerchat.channel-messages";

		ModeFlagMap *mode_flag_map = NULL;
		int num_mode_flags;
		ModeFlagMap local_mode_flag_map[] = {
			{EChannelMode_NoOutsideMessages, 'n'},
			{EChannelMode_TopicProtect, 't'},
			{EChannelMode_Moderated, 'm'},
			{EChannelMode_Private, 'p'},
			{EChannelMode_Secret, 's'},
			{EChannelMode_InviteOnly, 'i'},
			{EChannelMode_StayOpen, 'z'},
			{EChannelMode_Registered, 'r'},
			{EChannelMode_OpsObeyChannelLimit, 'e'}
		};
        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
			mode_flag_map = (ModeFlagMap*)&local_mode_flag_map;
			num_mode_flags = sizeof(local_mode_flag_map) / sizeof(ModeFlagMap);
            TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PeerchatBackendRequest, TaskThreadData>(OS::g_numAsync, server);
            scheduler->AddRequestHandler(EPeerchatRequestType_SetUserDetails, Perform_SetUserDetails);
            scheduler->AddRequestHandler(EPeerchatRequestType_SendMessageToTarget, Perform_SendMessageToTarget);
            scheduler->AddRequestHandler(EPeerchatRequestType_UserJoinChannel, Perform_UserJoinChannel);
            scheduler->AddRequestHandler(EPeerchatRequestType_UserPartChannel, Perform_UserPartChannel);
			scheduler->AddRequestHandler(EPeerchatRequestType_LookupChannelDetails, Perform_LookupChannelDetails);
			scheduler->AddRequestHandler(EPeerchatRequestType_LookupUserDetailsByName, Perform_LookupUserDetailsByName);
			scheduler->AddRequestHandler(EPeerchatRequestType_UpdateChannelModes, Perform_UpdateChannelModes);
			scheduler->AddRequestHandler(EPeerchatRequestType_ListChannels, Perform_ListChannels);
			scheduler->AddRequestHandler(EPeerchatRequestType_SetChannelKeys, Perform_SetChannelKeys);
			scheduler->AddRequestHandler(EPeerchatRequestType_GetChannelKeys, Perform_GetChannelKeys);

			scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_client_message_routingkey, Handle_Message);
            scheduler->AddRequestListener(peerchat_channel_exchange, peerchat_channel_message_routingkey, Handle_ChannelMessage);
			scheduler->DeclareReady();

            return scheduler;
        }
}