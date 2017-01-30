#ifndef _GP_BACKEND_H
#define _GP_BACKEND_H
#include <GP/server/GPPeer.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#include <OS/GPShared.h>

/********
persisttype_t
There are 4 types of persistent data stored for each player:
pd_private_ro: Readable only by the authenticated client it belongs to, can only by set on the server
pd_private_rw: Readable only by the authenticated client it belongs to, set by the authenticated client it belongs to
pd_public_ro: Readable by any client, can only be set on the server
pd_public_rw: Readable by any client, set by the authenicated client is belongs to
*********/
typedef enum {pd_private_ro, pd_private_rw, pd_public_ro, pd_public_rw} persisttype_t;

namespace GP {
	class Peer;
}

namespace GPBackend {



	enum EPersistRequestType {
		EPersistRequestType_SetUserData,
		EPersistRequestType_GetUserData,
		EPersistRequestType_NewGame,
		EPersistRequestType_UpdateGame,
	};

	
	enum EPersistBackendRespType {
		EPersistBackendRespType_NewGame,
		EPersistBackendRespType_UpdateGame,
		EPersistBackendRespType_SetUserData,
		EPersistBackendRespType_GetUserData,
	};

	typedef struct {
		std::string game_instance_identifier;
		EPersistBackendRespType type;
	} PersistBackendResponse;

	typedef void (*PersistBackendCallback)(bool success, PersistBackendResponse response_data, GP::Peer *peer, void* extra);



	typedef struct {
		EPersistRequestType type;

		GP::Peer *mp_peer;
		void* 	  mp_extra;
		PersistBackendCallback callback;

		std::map<std::string, std::string> kvMap;
		std::vector<std::string> keyList;
		std::string game_instance_identifier;
		int profileid;

		persisttype_t data_type;
		int data_index;
		bool data_kv_set;
	} PersistBackendRequest;


	class PersistBackendTask : public OS::Task<PersistBackendRequest> {
		public:
			PersistBackendTask();
			~PersistBackendTask();
			static PersistBackendTask *getPersistBackendTask();
			static void SubmitNewGameSession(GP::Peer *peer, void* extra, PersistBackendCallback cb);
			static void SubmitUpdateGameSession(std::map<std::string, std::string> kvMap, GP::Peer *peer, void* extra, std::string game_instance_identifier, PersistBackendCallback cb);
			static void SubmitSetPersistData(int profileid, GP::Peer *peer, void* extra, PersistBackendCallback cb, std::string data_b64_buffer, persisttype_t type, int index, bool kv_set);
			static void SubmitGetPersistData(int profileid, GP::Peer *peer, void *extra, PersistBackendCallback cb, persisttype_t type, int index, std::vector<std::string> keyList);

		private:
			static PersistBackendTask *m_task_singleton;
			static void *TaskThread(OS::CThread *thread);

			void PerformNewGameSession(PersistBackendRequest req);
			void PerformUpdateGameSession(PersistBackendRequest req);
			void PerformSetPersistData(PersistBackendRequest req);
			void PerformGetPersistData(PersistBackendRequest req);


			redisContext *mp_redis_connection;
			redisAsyncContext *mp_redis_subscribe_connection;
	};
}
#endif //_GP_BACKEND_H