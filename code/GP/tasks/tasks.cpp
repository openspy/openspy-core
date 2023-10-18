#include "tasks.h"
#include <rabbitmq-c/tcp_socket.h>

namespace GP {
	const char *gp_channel_exchange = "presence.core";
	const char *gp_client_message_routingkey = "presence.buddies";

	typedef struct {
		const char *amqp_exchange;
		const char *amqp_routing_key;
		bool (*amqp_event_callback)(TaskThreadData *, std::string);
		uv_thread_t amqp_authevent_consumer_thread;
		amqp_connection_state_t amqp_listener_conn;
		amqp_socket_t *amqp_socket;
	} ListenerArgs;

	ListenerArgs consume_presence_message = {gp_channel_exchange, gp_client_message_routingkey, Handle_PresenceMessage};
	ListenerArgs consume_authevent_message = {"openspy.core", "auth.events", Handle_AuthEvent};


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



		//setup generic presence listener
		consume_presence_message.amqp_listener_conn = amqp_new_connection();
		consume_presence_message.amqp_socket = amqp_tcp_socket_new(consume_presence_message.amqp_listener_conn);
		int status = amqp_socket_open(consume_presence_message.amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			perror("error opening presence amqp listener socket");
		}
		amqp_login(consume_presence_message.amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(consume_presence_message.amqp_listener_conn, 1);
		uv_thread_create(&consume_presence_message.amqp_authevent_consumer_thread, amqp_do_consume, &consume_presence_message);

	
	
		//setup auth event listener
		consume_authevent_message.amqp_listener_conn = amqp_new_connection();
		consume_authevent_message.amqp_socket = amqp_tcp_socket_new(consume_authevent_message.amqp_listener_conn);
		status = amqp_socket_open(consume_authevent_message.amqp_socket, address_buffer, atoi(port_buffer));
		if(status) {
			perror("error opening auth event amqp listener socket");
		}
		amqp_login(consume_authevent_message.amqp_listener_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user_buffer, pass_buffer);
		amqp_channel_open(consume_authevent_message.amqp_listener_conn, 1);
		uv_thread_create(&consume_authevent_message.amqp_authevent_consumer_thread, amqp_do_consume, &consume_authevent_message);
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