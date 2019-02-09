#include "tasks.h"
namespace GP {
        const char *gp_channel_exchange = "presence.core";
        const char *gp_client_message_routingkey = "presence.buddies";
        TaskScheduler<GPBackendRedisRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<GPBackendRedisRequest, TaskThreadData> *scheduler = new TaskScheduler<GPBackendRedisRequest, TaskThreadData>(4, server);
			scheduler->AddRequestHandler(EGPRedisRequestType_AuthorizeAdd, Perform_ToFromProfileAction);
			scheduler->AddRequestHandler(EGPRedisRequestType_AddBlock, Perform_ToFromProfileAction);
			scheduler->AddRequestHandler(EGPRedisRequestType_DelBlock, Perform_ToFromProfileAction);
			scheduler->AddRequestHandler(EGPRedisRequestType_Auth_NickEmail_GPHash, Perform_Auth_NickEmail_GPHash);
			scheduler->AddRequestHandler(EGPRedisRequestType_Auth_Uniquenick_GPHash, Perform_Auth_Uniquenick_GPHash);

            scheduler->AddRequestHandler(EGPRedisRequestType_AddBuddy, Perform_BuddyRequest);
            scheduler->AddRequestHandler(EGPRedisRequestType_UpdateStatus, Perform_SetPresenceStatus);
            scheduler->AddRequestHandler(EGPRedisRequestType_DelBuddy, Perform_ToFromProfileAction);
            //scheduler->AddRequestHandler(EGPRedisRequestType_RevokeAuth, Perform_ToFromProfileAction);
            scheduler->AddRequestHandler(EGPRedisRequestType_BuddyMessage, Perform_SendBuddyMessage);

            scheduler->AddRequestHandler(EGPRedisRequestType_LookupBuddyStatus, Perform_GetBuddyStatus);
			scheduler->AddRequestHandler(EGPRedisRequestType_LookupBlockStatus, Perform_GetBuddyStatus);
			scheduler->AddRequestHandler(EGPRedisRequestType_Auth_PreAuth_Token_GPHash, Perform_Auth_PreAuth_Token_GPHash);
            scheduler->AddRequestListener(gp_channel_exchange, gp_client_message_routingkey, Handle_PresenceMessage);
			scheduler->DeclareReady();

            return scheduler;
        }

		void GPReq_InitCurl(void *curl, char *post_data, void *write_data, GPBackendRedisRequest request) {
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
				url += "/v1/Presence/Status/FindBuddyStatuses";
				break;
			case EGPRedisRequestType_SendGPBlockStatus:
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

			/* Close socket after one use */
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TaskShared::curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);

		}
}