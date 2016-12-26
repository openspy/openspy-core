#include <stdio.h>
#include "GPBackend.h"
#include "GPDriver.h"
#include <stdlib.h>
#include <string.h>
#include <OS/socketlib/socketlib.h>

#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/hiredis.h>
#include <hiredis/adapters/libevent.h>
#include <event.h>
#undef _WINSOCK2API_

#include <OS/OpenSpy.h>
#include <OS/Thread.h>

#include <OS/legacy/helpers.h>

#include <sstream>

#include <OS/Search/Profile.h>
#include <curl/curl.h>
#include <jansson.h>
#include <jwt/jwt.h>

#define GP_BACKEND_REDIS_DB 5
#define BUDDY_ADDREQ_EXPIRETIME 604800
#define GP_STATUS_EXPIRE_TIME 3600
namespace GPBackend {
	struct curl_data {
	    json_t *json_data;
	};

	redisContext *mp_redis_connection = NULL;
	redisAsyncContext *mp_redis_subscribe_connection = NULL;

	GPBackendRedisTask *GPBackendRedisTask::m_task_singleton = NULL;

	const char *gp_buddies_channel = "presence.buddies";
	/* callback for curl fetch */
	size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));

		jwt_decode(&jwt, (const char *)contents, NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);

		if(json) {
			data->json_data = json_loads(json, 0, NULL);
			free(json);
		} else {
			data->json_data = NULL;
		}
		jwt_free(jwt);
	    return realsize;
	}

	void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata) {
	    redisReply *r = (redisReply*)reply;

	    if (reply == NULL) return;
	    int to_profileid, from_profileid;
	    char msg_type[16], reason[GP_REASON_LEN + 1];
	    GP::Peer *peer = NULL;
	    if (r->type == REDIS_REPLY_ARRAY) {
	    	if(r->elements == 3 && r->element[2]->type == REDIS_REPLY_STRING) {
	    		printf("Redis got: %s %s\n", r->element[1]->str, r->element[2]->str);
	    		if(strcmp(r->element[1]->str,gp_buddies_channel) == 0) {
	    			if(!find_param("type", r->element[2]->str,(char *)&msg_type, sizeof(msg_type))) {
	    					return;
	    			}
	    			find_param("reason", r->element[2]->str,(char *)&reason, sizeof(reason)-1);
	    			if(!strcmp(msg_type, "add_request")) {
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);;
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(to_profileid);
	    				if(peer) {
	    					peer->send_add_buddy_request(from_profileid, reason);
	    				}
	    			} else if(!strcmp(msg_type, "authorize_add")) {
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(from_profileid);
	    				if(peer) {
	    					peer->send_authorize_add(to_profileid);
	    				}
	    			} else if(!strcmp(msg_type, "status_update")) {
	    				GPStatus status;
	    				from_profileid = find_paramint("profileid", r->element[2]->str);
	    				status.status = (GPEnum)find_paramint("status", r->element[2]->str);
	    				find_param("status_string", r->element[2]->str,(char *)&status.status_str, sizeof(status.status_str));
	    				find_param("location_string", r->element[2]->str,(char *)&status.location_str, sizeof(status.location_str));
	    				status.quiet_flags = (GPEnum)find_paramint("quiet_flags", r->element[2]->str);
	    				find_param("ip", r->element[2]->str,(char *)&reason, sizeof(reason)-1);
	    				status.address.ip = Socket::htonl(Socket::inet_addr(OS::strip_quotes(reason).c_str()));
	    				status.address.port = Socket::htons(find_paramint("port", r->element[2]->str));
	    				GP::g_gbl_gp_driver->InformStatusUpdate(from_profileid, status);
	    			} else if(!strcmp(msg_type, "del_buddy")) {
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(to_profileid);
	    				if(peer) {
	    					peer->send_revoke_message(from_profileid, 0);
	    				}
	    			} else if(!strcmp(msg_type, "buddy_message")) {
	    				char type = find_paramint("msg_type", r->element[2]->str);
						int timestamp = find_paramint("timestamp", r->element[2]->str);
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);
	    				find_param("message", r->element[2]->str,(char *)&reason, sizeof(reason)-1);
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(to_profileid);
	    				if(peer) {
	    					peer->send_buddy_message(type, from_profileid, timestamp, reason);
	    				}
	    			} else if(!strcmp(msg_type, "block_buddy")) {
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(to_profileid);
	    				if(peer) {
	    					peer->send_user_blocked(from_profileid);
	    				}
	    				
	    			} else if(!strcmp(msg_type, "del_block_buddy")) {
	    				to_profileid = find_paramint("to_profileid", r->element[2]->str);
	    				from_profileid = find_paramint("from_profileid", r->element[2]->str);
	    				peer = GP::g_gbl_gp_driver->FindPeerByProfileID(to_profileid);
	    				if(peer) {
	    					peer->send_user_block_deleted(from_profileid);
	    				}
	    			}
	    		}
	    	}
	    }
	}
	void *setup_redis_async_sub(OS::CThread *) {
		struct event_base *base_sub = event_base_new();

	    mp_redis_subscribe_connection = redisAsyncConnect("127.0.0.1", 6379);
	    
	    redisLibeventAttach(mp_redis_subscribe_connection, base_sub);

	    redisAsyncCommand(mp_redis_subscribe_connection, onRedisMessage, NULL, "SUBSCRIBE %s",gp_buddies_channel);

	    event_base_dispatch(base_sub);
	}

	GPBackendRedisTask::GPBackendRedisTask() {
		OS::CreateThread(setup_redis_async_sub, NULL, true);

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = redisConnectWithTimeout("127.0.0.1", 6379, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(GPBackendRedisTask::TaskThread, this, true);
	}
	GPBackendRedisTask::~GPBackendRedisTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	GPBackendRedisTask *GPBackendRedisTask::getGPBackendRedisTask() {
		if(!GPBackendRedisTask::m_task_singleton) {
			GPBackendRedisTask::m_task_singleton = new GPBackendRedisTask();
		}
		return GPBackendRedisTask::m_task_singleton;
	}
	void GPBackendRedisTask::MakeBuddyRequest(int from_profileid, int to_profileid, const char *reason) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_BuddyRequest;
		req.uReqData.BuddyRequest.from_profileid = from_profileid;
		req.uReqData.BuddyRequest.to_profileid = to_profileid;

		int len = strlen(reason);
		if(len > GP_REASON_LEN)
			len = GP_REASON_LEN;

		strncpy(req.uReqData.BuddyRequest.reason, reason, len);
		req.uReqData.BuddyRequest.reason[len] = 0;

		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::MakeDelBuddyRequest(int adding_target, int adding_source) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_DelBuddy;
		req.uReqData.DelBuddy.from_profileid = adding_source;
		req.uReqData.DelBuddy.to_profileid = adding_target;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::MakeRevokeAuthRequest(int adding_target, int adding_source) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_RevokeAuth;
		req.uReqData.DelBuddy.from_profileid = adding_source;
		req.uReqData.DelBuddy.to_profileid = adding_target;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::MakeAuthorizeBuddyRequest(int adding_target, int adding_source) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_AuthorizeAdd;
		req.uReqData.AuthorizeAdd.from_profileid = adding_source;
		req.uReqData.AuthorizeAdd.to_profileid = adding_target;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::SetPresenceStatus(int from_profileid, GPStatus status, GP::Peer *peer) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_UpdateStatus;
		req.uReqData.StatusInfo = status;
		req.extra = (void *)peer;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void *GPBackendRedisTask::TaskThread(OS::CThread *thread) {
		GPBackendRedisTask *task = (GPBackendRedisTask *)thread->getParams();
		for(;;) {
			if(task->m_request_list.size() > 0) {
				std::vector<GPBackendRedisRequest>::iterator it = task->m_request_list.begin();
				task->mp_mutex->lock();
				while(it != task->m_request_list.end()) {
					GPBackendRedisRequest task_params = *it;
					switch(task_params.type) {
						case EGPRedisRequestType_BuddyRequest:
							task->Perform_BuddyRequest(task_params.uReqData.BuddyRequest);
						break;
						case EGPRedisRequestType_AuthorizeAdd:
							task->Perform_AuthorizeAdd(task_params.uReqData.AuthorizeAdd);
						break;
						case EGPRedisRequestType_UpdateStatus:
							task->Perform_SetPresenceStatus(task_params.uReqData.StatusInfo, task_params.extra);
						break;
						case EGPRedisRequestType_DelBuddy:
							task->Perform_DelBuddy(task_params.uReqData.DelBuddy, false);
						break;
						case EGPRedisRequestType_RevokeAuth:
							task->Perform_DelBuddy(task_params.uReqData.DelBuddy, true);
						break;
						case EGPRedisRequestType_SendLoginEvent:
							task->Perform_SendLoginEvent((GP::Peer *)task_params.extra);
						break;
						case EGPRedisRequestType_BuddyMessage:
							task->Perform_SendBuddyMessage((GP::Peer *)task_params.extra, task_params.uReqData.BuddyMessage);
						break;
						case EGPRedisRequestType_AddBlock:
							task->Perform_BlockBuddy(task_params.uReqData.BlockMessage);
						break;
						case EGPRedisRequestType_DelBlock:
							task->Perform_DelBuddyBlock(task_params.uReqData.BlockMessage);
						break;
						case EGPRedisRequestType_SendGPBuddyStatus:
							task->Perform_SendGPBuddyStatus((GP::Peer *)task_params.extra);
						break;
					}
					it = task->m_request_list.erase(it);
					continue;
				}
				task->mp_mutex->unlock();
			}
			OS::Sleep(TASK_SLEEP_TIME);
		}
		return NULL;
	}
	void GPBackendRedisTask::Perform_SetPresenceStatus(GPStatus status, void *extra) {
		GP::Peer *peer = (GP::Peer *)extra;
		int profileid = peer->GetProfileID();
		redisAppendCommand(this->mp_redis_connection, "SELECT %d", GP_BACKEND_REDIS_DB);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d status %d", profileid, status.status);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d status_string %s", profileid, status.status_str);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d location_string %s", profileid, status.location_str);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d quiet_flags %d", profileid, status.quiet_flags);

		struct sockaddr_in addr;
		addr.sin_port = Socket::htons(status.address.port);
		addr.sin_addr.s_addr = (status.address.ip);
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d ip %s", profileid, ipinput);
		redisAppendCommand(this->mp_redis_connection, "HSET status_%d port %d", profileid, addr.sin_port);
		redisAppendCommand(this->mp_redis_connection, "EXPIRE status_%d %d", profileid, GP_STATUS_EXPIRE_TIME);

		redisAppendCommand(this->mp_redis_connection, "PUBLISH %s \\type\\status_update\\profileid\\%d\\status_string\\%s\\status\\%d\\location_string\\%s\\quiet_flags\\%d\\ip\\%s\\port\\%d",
		 gp_buddies_channel, profileid,status.status_str,status.status,status.location_str,
		 status.quiet_flags,ipinput,addr.sin_port); //TODO: escape this

		for(int i=0;i<9;i++) {
			redisReply *reply;
			int r = redisGetReply(this->mp_redis_connection, (void **) &reply );
			freeReplyObject(reply);
		}		
	}
	void GPBackendRedisTask::Perform_BuddyRequest(struct sBuddyRequest request) {
		redisAppendCommand(this->mp_redis_connection, "SELECT %d", GP_BACKEND_REDIS_DB);
		redisAppendCommand(this->mp_redis_connection, "HSET add_req_%d %d %s", request.to_profileid, request.from_profileid, request.reason);
		redisAppendCommand(this->mp_redis_connection, "EXPIRE add_req_%d %d", request.to_profileid, BUDDY_ADDREQ_EXPIRETIME);

		redisAppendCommand(this->mp_redis_connection, "PUBLISH %s \\type\\add_request\\from_profileid\\%d\\to_profileid\\%d\\reason\\%s", gp_buddies_channel, request.from_profileid, request.to_profileid, request.reason); //TODO: escape this

		for(int i=0;i<4;i++) {
			redisReply *reply;
			int r = redisGetReply(this->mp_redis_connection, (void **) &reply );
			freeReplyObject(reply);
		}
	}
	void GPBackendRedisTask::Perform_AuthorizeAdd(struct sAuthorizeAdd request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("authorize_buddy_add"));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.from_profileid));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.to_profileid));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}

		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}

		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
	void GPBackendRedisTask::Perform_DelBuddy(struct sDelBuddy request, bool send_revoke) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("del_buddy"));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.from_profileid));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.to_profileid));
		json_object_set_new(send_obj, "send_revoke", send_revoke ? json_true() : json_false());


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}
		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}

		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);	
	}
	void GPBackendRedisTask::SendLoginEvent(GP::Peer *peer) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_SendLoginEvent;
		req.extra = (void *)peer;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);

		req.type = EGPRedisRequestType_SendGPBuddyStatus;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);

	}
	void GPBackendRedisTask::Perform_SendLoginEvent(GP::Peer *peer) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("send_presence_login_messages"));
		json_object_set_new(send_obj, "profileid", json_integer(peer->GetProfileID()));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}
		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}
		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
	void GPBackendRedisTask::SendMessage(GP::Peer *peer, int to_profileid, char msg_type, const char *message) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_BuddyMessage;
		req.extra = (void *)peer;
		req.uReqData.BuddyMessage.to_profileid = to_profileid;


		int len = strlen(message);
		if(len > GP_REASON_LEN)
			len = GP_REASON_LEN;

		strncpy(req.uReqData.BuddyMessage.message, message, len);
		req.uReqData.BuddyMessage.message[len] = 0;

		req.uReqData.BuddyMessage.type = msg_type;

		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::Perform_SendBuddyMessage(GP::Peer *peer, struct sBuddyMessage msg) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("send_buddy_message"));
		json_object_set_new(send_obj, "to_profileid", json_integer(msg.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(peer->GetProfileID()));
		json_object_set_new(send_obj, "type", json_integer(msg.type));
		json_object_set_new(send_obj, "message", json_string(msg.message));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}

		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}
		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
	void GPBackendRedisTask::MakeBlockRequest(int from_profileid, int block_id) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_AddBlock;
		req.uReqData.BlockMessage.from_profileid = from_profileid;
		req.uReqData.BlockMessage.to_profileid = block_id;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::MakeRemoveBlockRequest(int from_profileid, int block_id) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_DelBlock;
		req.uReqData.BlockMessage.from_profileid = from_profileid;
		req.uReqData.BlockMessage.to_profileid = block_id;
		GPBackendRedisTask::getGPBackendRedisTask()->AddRequest(req);
	}
	void GPBackendRedisTask::Perform_BlockBuddy(struct sBlockBuddy msg) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("block_buddy"));
		json_object_set_new(send_obj, "to_profileid", json_integer(msg.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(msg.from_profileid));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}

		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}
		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
	void GPBackendRedisTask::Perform_DelBuddyBlock(struct sBlockBuddy msg) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("del_block_buddy"));
		json_object_set_new(send_obj, "to_profileid", json_integer(msg.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(msg.from_profileid));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);
		}

		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}
		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
	void GPBackendRedisTask::load_and_send_gpstatus(GP::Peer *peer, json_t *json) {
		GPStatus status;
		json_t* json_obj = json_object_get(json, "profileid");
		const char *str;
		int len;
		if(!json_obj)
			return;
		int profileid = json_integer_value(json_obj);

		//[{'status': 1, 'location_string': '', 'ip': '10.10.10.1', 'status_string': 'Online', 'quiet_flags': 0, 'profileid': 10000, 'port': 28762}]
		json_obj = json_object_get(json, "status_string");
		if(!json_obj)
			return;
		str = json_string_value(json_obj);
		len = strlen(str);
		if(len > GP_STATUS_STRING_LEN)
			len = GP_STATUS_STRING_LEN;
		strncpy(status.status_str, str, len);
		status.status_str[len] = 0;


		json_obj = json_object_get(json, "location_string");
		if(!json_obj)
			return;
		len = strlen(str);
		if(len > GP_LOCATION_STRING_LEN)
			len = GP_LOCATION_STRING_LEN;
		strncpy(status.location_str, str, len);
		status.location_str[len] = 0;

		status.quiet_flags = json_integer_value(json_obj);

		json_obj = json_object_get(json, "ip");
		if(!json_obj)
			return;

		str = json_string_value(json_obj);
		status.address.ip = Socket::htonl(Socket::inet_addr(str));

		json_obj = json_object_get(json, "port");
		if(!json_obj)
			return;
		len = json_integer_value(json_obj);
		status.address.port = Socket::htons(len);

		peer->inform_status_update(profileid, status, true);

	}
	void GPBackendRedisTask::Perform_SendGPBuddyStatus(GP::Peer *peer) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("get_buddies_status"));
		json_object_set_new(send_obj, "profileid", json_integer(peer->GetProfileID()));


		CURL *curl = curl_easy_init();
		CURLcode res;
		bool success = false;

		char *json_data = json_dumps(send_obj, 0);

		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);


		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSGPBackendRedisTask");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			json_t *status_array = json_object_get(recv_data.json_data, "statuses");
			if(status_array) {
				int num_items = json_array_size(status_array);
				for(int i=0;i<num_items;i++) {
					json_t *status = json_array_get(status_array, i);
					load_and_send_gpstatus(peer, status);
				}
			}

		}

		if(recv_data.json_data) {
			json_decref(recv_data.json_data);
		}
		if(json_data)
			free((void *)json_data);

		if(send_obj)
			json_decref(send_obj);

		if(jwt_encoded)
			free((void *)jwt_encoded);
	}
}