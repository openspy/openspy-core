#include "tasks.h"
namespace GP {
        const char *gp_channel_exchange = "presence.core";
        const char *gp_client_message_routingkey = "presence.buddies";
		TaskScheduler<GPBackendRedisRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
			{EGPRedisRequestType_AuthorizeAdd, Perform_ToFromProfileAction},
			{EGPRedisRequestType_AddBlock, Perform_ToFromProfileAction},
			{EGPRedisRequestType_DelBlock, Perform_ToFromProfileAction},
			{EGPRedisRequestType_Auth_NickEmail_GPHash, Perform_Auth_NickEmail_GPHash},
			{EGPRedisRequestType_Auth_Uniquenick_GPHash, Perform_Auth_Uniquenick_GPHash},

			{EGPRedisRequestType_AddBuddy, Perform_BuddyRequest},
			{EGPRedisRequestType_UpdateStatus, Perform_SetPresenceStatus},
			{EGPRedisRequestType_DelBuddy, Perform_ToFromProfileAction},
			{EGPRedisRequestType_BuddyMessage, Perform_SendBuddyMessage},

			{EGPRedisRequestType_LookupBuddyStatus, Perform_GetBuddyStatus},
			{EGPRedisRequestType_LookupBlockStatus, Perform_GetBuddyStatus},
			{EGPRedisRequestType_Auth_PreAuth_Token_GPHash, Perform_Auth_PreAuth_Token_GPHash},
			{EGPRedisRequestType_Auth_LoginTicket, Perform_Auth_LoginTicket_GPHash},
			{NULL, NULL}
		};

		TaskScheduler<GPBackendRedisRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
			{gp_channel_exchange, gp_client_message_routingkey, Handle_PresenceMessage},
			{"openspy.core", "auth.events", Handle_AuthEvent},
			{NULL, NULL, NULL}
		};
        TaskScheduler<GPBackendRedisRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<GPBackendRedisRequest, TaskThreadData> *scheduler = new TaskScheduler<GPBackendRedisRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

			scheduler->DeclareReady();

            return scheduler;
        }

		void GPReq_InitCurl(void *curl, char *post_data, void *write_data, GPBackendRedisRequest request, struct curl_slist **out_list) {
			struct curl_slist *chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(OS::g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (request.peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + request.peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			std::string url = OS::g_webServicesURL;
			switch (request.type) {
			case EGPRedisRequestType_Auth_NickEmail_GPHash:
				url += "/v1/Presence/Auth/NickEmailAuth";
				break;
			case EGPRedisRequestType_Auth_Uniquenick_GPHash:
				url += "/v1/Presence/Auth/UniqueNickAuth";
				break;
			case EGPRedisRequestType_Auth_PreAuth_Token_GPHash:
				url += "/v1/Presence/Auth/PreAuth";
				break;
			case EGPRedisRequestType_Auth_LoginTicket:
				url += "/v1/Presence/Auth/LoginTicketAuth";
				break;
			case EGPRedisRequestType_AddBlock:
				url += "/v1/Presence/List/Block";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				break;
			case EGPRedisRequestType_DelBlock:
				url += "/v1/Presence/List/Block";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
			case EGPRedisRequestType_AddBuddy:
				url += "/v1/Presence/List/Buddy";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				break;
			case EGPRedisRequestType_DelBuddy:
				url += "/v1/Presence/List/Buddy";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
			case EGPRedisRequestType_AuthorizeAdd:
				url += "/v1/Presence/List/AuthorizeAdd";
				break;
			case EGPRedisRequestType_BuddyMessage:
				url += "/v1/Presence/List/SendMessage";
				break;
			case EGPRedisRequestType_SendGPBuddyStatus:
			case EGPRedisRequestType_LookupBuddyStatus:
				url += "/v1/Presence/Status/FindBuddyStatuses";
				break;
			case EGPRedisRequestType_SendGPBlockStatus:
			case EGPRedisRequestType_LookupBlockStatus:
				url += "/v1/Presence/Status/FindBlockStatuses";
				break;
			case EGPRedisRequestType_UpdateStatus:
				url += "/v1/Presence/Status/SetStatus";
				break;
			}
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSCoreAuth");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			/* enable TCP keep-alive for this transfer */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
			
			/* set keep-alive idle time to 120 seconds */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
			
			/* interval time between keep-alive probes: 60 seconds */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TaskShared::curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);

			if(out_list != NULL) {
				*out_list = chunk;
			}

		}
}