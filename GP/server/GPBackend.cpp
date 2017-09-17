#include <stdio.h>
#include "GPBackend.h"
#include "GPDriver.h"
#include "GPServer.h"
#include <stdlib.h>
#include <string.h>
#include <OS/socketlib/socketlib.h>


#include <OS/OpenSpy.h>
#include <OS/Thread.h>
#include <OS/KVReader.h>
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

	OS::TaskPool<GPBackendRedisTask, GPBackendRedisRequest> *m_task_pool = NULL;
	Redis::Connection *mp_redis_async_connection = NULL;
	OS::CThread *mp_async_thread = NULL;
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

	void GPBackendRedisTask::onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
		Redis::Value v = reply.values.front();

		GP::Server *server = (GP::Server *)privdata;

		std::string msg_type, reason;
		int to_profileid, from_profileid;
		GP::Peer *peer = NULL;

		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {

				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), gp_buddies_channel) == 0) {
					OS::KVReader reader = v.arr_value.values[2].second.value._str;
					msg_type = reader.GetValue("type");
					if (msg_type.compare("add_request") == 0) {
						reason = reader.GetValue("reason");
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
						if (peer) {
							peer->send_add_buddy_request(from_profileid, reason.c_str());
						}
					} else if (msg_type.compare("authorize_add") == 0) {
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						peer = (GP::Peer *)server->findPeerByProfile(from_profileid);
						if (peer) {
							peer->send_authorize_add(to_profileid);
						}
					} else if (msg_type.compare("status_update") == 0) {
						GPShared::GPStatus status;
						from_profileid = reader.GetValueInt("from_profileid");
						status.status = (GPShared::GPEnum)reader.GetValueInt("status");

						status.status_str = reader.GetValue("status_string");
						status.location_str = reader.GetValue("location_string");
						status.quiet_flags = (GPShared::GPEnum)reader.GetValueInt("quiet_flags");
						reason = reader.GetValue("ip");
						status.address.ip = Socket::htonl(Socket::inet_addr(OS::strip_quotes(reason).c_str()));
						status.address.port = Socket::htons(reader.GetValueInt("port"));
						server->InformStatusUpdate(from_profileid, status);
					} else if (msg_type.compare("del_buddy") == 0) {
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
						if (peer) {
							peer->send_revoke_message(from_profileid, 0);
						}
					} else if (msg_type.compare("buddy_message") == 0) {
						char type = reader.GetValueInt("msg_type");
						int timestamp = reader.GetValueInt("timestamp");
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						reason = reader.GetValue("message");
						peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
						if (peer) {
							peer->send_buddy_message(type, from_profileid, timestamp, reason.c_str());
						}
					} else if (msg_type.compare("block_buddy") == 0) {
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
						if (peer) {
							peer->send_user_blocked(from_profileid);
						}
					} else if (msg_type.compare("del_block_buddy") == 0) {
						to_profileid = reader.GetValueInt("to_profileid");
						from_profileid = reader.GetValueInt("from_profileid");
						peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
						if (peer) {
							peer->send_user_block_deleted(from_profileid);
						}
					}
				}
			}
		}
	}
	GPBackendRedisTask::GPBackendRedisTask() {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 3;

		mp_redis_connection = Redis::Connect(OS_REDIS_ADDR, t);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(GPBackendRedisTask::TaskThread, this, true);
	}
	GPBackendRedisTask::~GPBackendRedisTask() {
		delete mp_thread;
		delete mp_mutex;

		Redis::Disconnect(mp_redis_connection);
	}
	void GPBackendRedisTask::MakeBuddyRequest(GP::Peer *peer, int to_profileid, std::string reason) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_BuddyRequest;
		req.peer = peer;
		peer->IncRef();
		req.uReqData.BuddyRequest.from_profileid = peer->GetProfileID();
		req.uReqData.BuddyRequest.to_profileid = to_profileid;

		int len = reason.length();
		if(len > GP_REASON_LEN)
			len = GP_REASON_LEN;

		strncpy(req.uReqData.BuddyRequest.reason, reason.c_str(), len);
		req.uReqData.BuddyRequest.reason[len] = 0;

		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::MakeDelBuddyRequest(GP::Peer *peer, int target) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_DelBuddy;
		req.peer = peer;
		peer->IncRef();
		req.uReqData.DelBuddy.from_profileid = peer->GetProfileID();
		req.uReqData.DelBuddy.to_profileid = target;
		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::MakeRevokeAuthRequest(GP::Peer *peer, int target) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_RevokeAuth;
		req.peer = peer;
		peer->IncRef();
		req.uReqData.DelBuddy.from_profileid = peer->GetProfileID();
		req.uReqData.DelBuddy.to_profileid = target;
		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::MakeAuthorizeBuddyRequest(GP::Peer *peer, int target) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_AuthorizeAdd;
		req.peer = peer;
		peer->IncRef();
		req.uReqData.AuthorizeAdd.from_profileid = peer->GetProfileID();
		req.uReqData.AuthorizeAdd.to_profileid = target;
		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::SetPresenceStatus(int from_profileid, GPShared::GPStatus status, GP::Peer *peer) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_UpdateStatus;
		req.peer = peer;
		peer->IncRef();
		req.StatusInfo = status;
		req.extra = (void *)peer;
		m_task_pool->AddRequest(req);
	}
	void *GPBackendRedisTask::TaskThread(OS::CThread *thread) {
		GPBackendRedisTask *task = (GPBackendRedisTask *)thread->getParams();
		for (;;) {

			while (task->mp_thread_poller->wait()) {
				task->mp_mutex->lock();
				if (task->m_request_list.empty()) {
					task->mp_mutex->unlock();
					continue;
				}
				while (!task->m_request_list.empty()) {
					GPBackendRedisRequest task_params = task->m_request_list.front();
					task->mp_mutex->unlock();
					switch (task_params.type) {
					case EGPRedisRequestType_BuddyRequest:
						task->Perform_BuddyRequest(task_params);
						break;
					case EGPRedisRequestType_AuthorizeAdd:
						task->Perform_AuthorizeAdd(task_params);
						break;
					case EGPRedisRequestType_UpdateStatus:
						task->Perform_SetPresenceStatus(task_params);
						break;
					case EGPRedisRequestType_RevokeAuth:
					case EGPRedisRequestType_DelBuddy:
						task->Perform_DelBuddy(task_params);
						break;
					case EGPRedisRequestType_SendLoginEvent:
						task->Perform_SendLoginEvent(task_params);
						break;
					case EGPRedisRequestType_BuddyMessage:
						task->Perform_SendBuddyMessage(task_params);
						break;
					case EGPRedisRequestType_AddBlock:
						task->Perform_BlockBuddy(task_params);
						break;
					case EGPRedisRequestType_DelBlock:
						task->Perform_DelBuddyBlock(task_params);
						break;
					case EGPRedisRequestType_SendGPBuddyStatus:
						task->Perform_SendGPBuddyStatus(task_params);
						break;
					}

					task->mp_mutex->lock();
					if(task_params.peer)
						task_params.peer->DecRef();
					task->m_request_list.pop();
				}

				task->mp_mutex->unlock();
			}
		}
		return NULL;
	}
	void GPBackendRedisTask::Perform_SetPresenceStatus(GPBackendRedisRequest request) {
		int profileid = request.peer->GetProfileID();
		Redis::Command(mp_redis_connection, 0, "SELECT %d", GP_BACKEND_REDIS_DB);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d status %d", profileid, request.StatusInfo.status);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d status_string %s", profileid, request.StatusInfo.status_str);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d location_string %s", profileid, request.StatusInfo.location_str);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d quiet_flags %d", profileid, request.StatusInfo.quiet_flags);

		struct sockaddr_in addr;
		addr.sin_port = Socket::htons(request.StatusInfo.address.port);
		addr.sin_addr.s_addr = (request.StatusInfo.address.ip);
		const char *ipinput = Socket::inet_ntoa(addr.sin_addr);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d ip %s", profileid, ipinput);
		Redis::Command(mp_redis_connection, 0, "HSET status_%d port %d", profileid, addr.sin_port);
		Redis::Command(mp_redis_connection, 0, "EXPIRE status_%d %d", profileid, GP_STATUS_EXPIRE_TIME);

		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\type\\status_update\\profileid\\%d\\status_string\\%s\\status\\%d\\location_string\\%s\\quiet_flags\\%d\\ip\\%s\\port\\%d",
		 gp_buddies_channel, profileid, request.StatusInfo.status_str, request.StatusInfo.status, request.StatusInfo.location_str,
			request.StatusInfo.quiet_flags,ipinput,addr.sin_port); //TODO: escape this
	}
	void GPBackendRedisTask::Perform_BuddyRequest(GPBackendRedisRequest request) {
		Redis::Command(mp_redis_connection, 0, "SELECT %d", GP_BACKEND_REDIS_DB);
		Redis::Command(mp_redis_connection, 0, "HSET add_req_%d %d %s", request.uReqData.BuddyRequest.to_profileid, request.uReqData.BuddyRequest.from_profileid, request.uReqData.BuddyRequest.reason);
		Redis::Command(mp_redis_connection, 0, "EXPIRE add_req_%d %d", request.uReqData.BuddyRequest.to_profileid, BUDDY_ADDREQ_EXPIRETIME);

		Redis::Command(mp_redis_connection, 0, "PUBLISH %s \\type\\add_request\\from_profileid\\%d\\to_profileid\\%d\\reason\\%s", gp_buddies_channel, request.uReqData.BuddyRequest.from_profileid, request.uReqData.BuddyRequest.to_profileid, request.uReqData.BuddyRequest.reason); //TODO: escape this
	}
	void GPBackendRedisTask::Perform_AuthorizeAdd(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("authorize_buddy_add"));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.uReqData.BuddyRequest.from_profileid));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.uReqData.BuddyRequest.to_profileid));


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
	void GPBackendRedisTask::Perform_DelBuddy(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("del_buddy"));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.uReqData.BuddyRequest.from_profileid));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.uReqData.BuddyRequest.to_profileid));
		json_object_set_new(send_obj, "send_revoke", request.type == EGPRedisRequestType_RevokeAuth ? json_true() : json_false());


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
		req.peer = peer;
		peer->IncRef();
		m_task_pool->AddRequest(req);
		peer->IncRef();

		req.type = EGPRedisRequestType_SendGPBuddyStatus;
		m_task_pool->AddRequest(req);

	}
	void GPBackendRedisTask::Perform_SendLoginEvent(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("send_presence_login_messages"));
		json_object_set_new(send_obj, "profileid", json_integer(request.peer->GetProfileID()));


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
		req.peer = peer;
		peer->IncRef();
		req.uReqData.BuddyMessage.to_profileid = to_profileid;


		int len = strlen(message);
		if(len > GP_REASON_LEN)
			len = GP_REASON_LEN;

		strncpy(req.uReqData.BuddyMessage.message, message, len);
		req.uReqData.BuddyMessage.message[len] = 0;

		req.uReqData.BuddyMessage.type = msg_type;

		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::Perform_SendBuddyMessage(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("send_buddy_message"));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.uReqData.BuddyMessage.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.peer->GetProfileID()));
		json_object_set_new(send_obj, "type", json_integer(request.uReqData.BuddyMessage.type));
		json_object_set_new(send_obj, "message", json_string(request.uReqData.BuddyMessage.message));


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
	void GPBackendRedisTask::MakeBlockRequest(GP::Peer *peer, int block_id) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_AddBlock;
		peer->IncRef();
		req.peer = peer;
		req.uReqData.BlockMessage.from_profileid = peer->GetProfileID();
		req.uReqData.BlockMessage.to_profileid = block_id;
		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::MakeRemoveBlockRequest(GP::Peer *peer, int block_id) {
		GPBackendRedisRequest req;
		req.type = EGPRedisRequestType_DelBlock;
		peer->IncRef();
		req.peer = peer;
		req.uReqData.BlockMessage.from_profileid = peer->GetProfileID();
		req.uReqData.BlockMessage.to_profileid = block_id;
		m_task_pool->AddRequest(req);
	}
	void GPBackendRedisTask::Perform_BlockBuddy(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("block_buddy"));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.uReqData.BuddyRequest.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.uReqData.BuddyRequest.from_profileid));


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
	void GPBackendRedisTask::Perform_DelBuddyBlock(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("del_block_buddy"));
		json_object_set_new(send_obj, "to_profileid", json_integer(request.uReqData.BuddyRequest.to_profileid));
		json_object_set_new(send_obj, "from_profileid", json_integer(request.uReqData.BuddyRequest.from_profileid));


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
		GPShared::GPStatus status;
		json_t* json_obj = json_object_get(json, "profileid");
		const char *str;
		int len;
		if(!json_obj)
			return;
		int profileid = json_integer_value(json_obj);

		json_obj = json_object_get(json, "status");
		if(!json_obj)
			return;

		status.status = (GPShared::GPEnum)json_integer_value(json_obj);

		json_obj = json_object_get(json, "status_string");
		if(!json_obj)
			return;

		str = json_string_value(json_obj);
		if(str) {
			len = strlen(str);
			if(len > GP_STATUS_STRING_LEN)
				len = GP_STATUS_STRING_LEN;
			status.status_str = str;
		}


		json_obj = json_object_get(json, "location_string");
		if(!json_obj)
			return;

		str = json_string_value(json_obj);
		if(str) {
			len = strlen(str);
			if(len > GP_LOCATION_STRING_LEN)
				len = GP_LOCATION_STRING_LEN;
			status.location_str = str;
		} else {
			status.location_str[0] = 0;
		}


		json_obj = json_object_get(json, "quiet_flags");
		if(!json_obj)
			return;
		status.quiet_flags = json_integer_value(json_obj);

		json_obj = json_object_get(json, "ip");
		if(!json_obj)
			return;

		str = json_string_value(json_obj);
		if(str)
			status.address.ip = Socket::htonl(Socket::inet_addr(str));

		json_obj = json_object_get(json, "port");
		if(!json_obj)
			return;
		len = json_integer_value(json_obj);
		status.address.port = Socket::htons(len);

		peer->inform_status_update(profileid, status, true);

	}
	void GPBackendRedisTask::Perform_SendGPBuddyStatus(GPBackendRedisRequest request) {
		curl_data recv_data;
		json_t *send_obj = json_object();
		json_object_set_new(send_obj, "mode", json_string("get_buddies_status"));
		json_object_set_new(send_obj, "profileid", json_integer(request.peer->GetProfileID()));


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
					load_and_send_gpstatus(request.peer, status);
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
	void GPBackendRedisTask::AddDriver(GP::Driver *driver) {
		if (std::find(m_drivers.begin(), m_drivers.end(), driver) == m_drivers.end()) {
			m_drivers.push_back(driver);
		}
	}
	void GPBackendRedisTask::RemoveDriver(GP::Driver *driver) {
		std::vector<GP::Driver *>::iterator it = std::find(m_drivers.begin(), m_drivers.end(), driver);
		if (it != m_drivers.end()) {
			m_drivers.erase(it);
		}
	}
	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;
		mp_redis_async_connection = Redis::Connect(OS_REDIS_ADDR, t);
		
		Redis::LoopingCommand(mp_redis_async_connection, 60, GPBackendRedisTask::onRedisMessage, thread->getParams(), "SUBSCRIBE %s", gp_buddies_channel);
		return NULL;
	}

	void SetupTaskPool(GP::Server *server) {

		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;

		mp_async_thread = OS::CreateThread(setup_redis_async, server, true);
		OS::Sleep(200);

		m_task_pool = new OS::TaskPool<GPBackendRedisTask, GPBackendRedisRequest>(NUM_PRESENCE_THREADS);
		server->SetTaskPool(m_task_pool);
	}

	void ShutdownTaskPool() {
		delete m_task_pool;
	}
}
