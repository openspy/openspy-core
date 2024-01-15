#include "tasks.h"
#ifdef LEGACYRABBITMQ
	#include <amqp_tcp_socket.h>
#else
	#include <rabbitmq-c/tcp_socket.h>
#endif

#include <OS/tasks.h>

namespace GP {
	const char *gp_channel_exchange = "presence.core";
	const char *gp_client_message_routingkey = "presence.buddies";

	TaskShared::ListenerEventHandler consume_presence_handler = {gp_channel_exchange, gp_client_message_routingkey, Handle_PresenceMessage};
	TaskShared::ListenerEventHandler consume_authevent_handler = {"openspy.core", "auth.events", Handle_AuthEvent};
	TaskShared::ListenerEventHandler all_events[] = {consume_presence_handler, consume_authevent_handler};
	TaskShared::ListenerArgs gp_event_handler = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};

	void InitTasks() {
		setup_listener(&gp_event_handler);
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
		curl_easy_setopt(curl, CURLOPT_SHARE, OS::g_curlShare);

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

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		GPBackendRedisRequest *work_data = (GPBackendRedisRequest *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->type) {
			case EGPRedisRequestType_AuthorizeAdd:
			case EGPRedisRequestType_AddBlock:
			case EGPRedisRequestType_DelBlock:
			case EGPRedisRequestType_DelBuddy:
				Perform_ToFromProfileAction(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_Auth_NickEmail_GPHash:
				Perform_Auth_NickEmail_GPHash(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_Auth_Uniquenick_GPHash:
				Perform_Auth_Uniquenick_GPHash(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_AddBuddy:
				Perform_BuddyRequest(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_UpdateStatus:
				Perform_SetPresenceStatus(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_BuddyMessage:
				Perform_SendBuddyMessage(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_LookupBuddyStatus:
			case EGPRedisRequestType_LookupBlockStatus:
				Perform_GetBuddyStatus(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_Auth_PreAuth_Token_GPHash:
				Perform_Auth_PreAuth_Token_GPHash(*work_data, &temp_data);
			break;
			case EGPRedisRequestType_Auth_LoginTicket:
				Perform_Auth_LoginTicket_GPHash(*work_data, &temp_data);
			break;
		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		GPBackendRedisRequest *work_data = (GPBackendRedisRequest *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}

	void AddGPTaskRequest(GPBackendRedisRequest request) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		GPBackendRedisRequest *work_data = new GPBackendRedisRequest();
		*work_data = request;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);
		uv_queue_work(uv_default_loop(), uv_req, PerformUVWorkRequest, PerformUVWorkRequestCleanup);
	}
}
