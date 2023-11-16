#include "tasks.h"
#include <server/GSPeer.h>
namespace GS {
	typedef struct {
		bool success;
		PersistBackendResponse response_data;
		GS::Peer *peer;
		void* extra;
		PersistBackendCallback callback;

		OS::User user;
		OS::Profile profile;
		TaskShared::AuthData auth_data;
		TaskShared::AuthCallback authCallback;

	} PersistCallbackArgs;
	uv_async_t m_async_callback_handle;
	std::queue<PersistCallbackArgs> m_callback_responses;
	uv_mutex_t m_callback_mutex;


	void uvworker_callback_dispatcher(uv_async_t *handle);

	void InitTasks() {
    	uv_async_init(uv_default_loop(), &m_async_callback_handle, uvworker_callback_dispatcher);
    	uv_mutex_init(&m_callback_mutex);
	}

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		PersistBackendRequest *request = (PersistBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(request->type) {
			case EPersistRequestType_SetUserData:
				Perform_SetUserData(*request, &temp_data);
			break;
			case EPersistRequestType_GetUserData:
				Perform_GetUserData(*request, &temp_data);
			break;
			case EPersistRequestType_GetUserKeyedData:
				Perform_GetUserKeyedData(*request, &temp_data);
			break;
			case EPersistRequestType_NewGame:
				Perform_NewGame(*request, &temp_data);
			break;
			case EPersistRequestType_UpdateGame:
				Perform_UpdateGame(*request, &temp_data);
			break;
			case EPersistRequestType_GetGameInfoByGamename:
				Perform_GetGameInfo(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_ProfileID:
				Perform_ProfileIDAuth(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_AuthTicket:
				Perform_AuthTokenAuth(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_CDKey:
				Perform_CDKeyAuth(*request, &temp_data);
			break;
			case EPersistRequestType_GetProfileIDFromCDKey:
				Perform_GetProfileIDFromCD(*request, &temp_data);
			break;
		}
		maybe_dispatch_responses();
	}

	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		PersistBackendRequest *work_data = (PersistBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);		
	}

	void callback_dispatch_later(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra, PersistBackendCallback callback) {
		uv_mutex_lock(&m_callback_mutex);
		PersistCallbackArgs args;
		args.success = success;
		args.peer = peer;
		args.extra = extra;
		args.callback = callback;
		args.response_data = response_data;
        args.authCallback = NULL;
		m_callback_responses.push(args);
		uv_mutex_unlock(&m_callback_mutex);

		uv_async_send(&m_async_callback_handle);
	}

	void uvworker_callback_dispatcher(uv_async_t *handle) {
		uv_mutex_lock(&m_callback_mutex);
		if(!m_callback_responses.empty()) {
			PersistCallbackArgs args = m_callback_responses.front();
			if(args.callback != NULL) {
				args.callback(args.success, args.response_data, args.peer, args.extra);
			} else if(args.authCallback != NULL) {
				args.authCallback(args.success, args.user, args.profile, args.auth_data, args.extra, args.peer);
			}
			if(args.peer) {
				args.peer->DecRef();
			}
			m_callback_responses.pop();
		}
		uv_mutex_unlock(&m_callback_mutex);
	}

	void authcallback_dispatch_later(bool success, OS::User user, OS::Profile profile, 
		TaskShared::AuthData auth_data, void *extra, INetPeer *peer, TaskShared::AuthCallback authCallback) {
		uv_mutex_lock(&m_callback_mutex);
		PersistCallbackArgs args;
		args.success = success;
		args.peer = (GS::Peer *)peer;
		args.extra = extra;
		args.auth_data = auth_data;
		args.user = user;
		args.profile = profile;
		args.authCallback = authCallback;
        args.callback = NULL;
		m_callback_responses.push(args);
		uv_mutex_unlock(&m_callback_mutex);
		
		uv_async_send(&m_async_callback_handle);
	}
	void maybe_dispatch_responses() {
		bool do_dispatch = false;
		uv_mutex_lock(&m_callback_mutex);
		if(!m_callback_responses.empty()) {
			do_dispatch = true;
		}
		uv_mutex_unlock(&m_callback_mutex);
		
		if(do_dispatch) {
			uv_async_send(&m_async_callback_handle);
		}

	}
}
