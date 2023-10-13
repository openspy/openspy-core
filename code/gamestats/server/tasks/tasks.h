#ifndef _GS_TASKS_H
#define _GS_TASKS_H
#include <string>

#include <OS/KVReader.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/MessageQueue/MQInterface.h>

#include <OS/GPShared.h>

#include <OS/SharedTasks/tasks.h>
#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/ProfileTasks.h>
namespace GS {
	class Peer;
	class Driver;

	/********
	persisttype_t
	There are 4 types of persistent data stored for each player:
	pd_private_ro: Readable only by the authenticated client it belongs to, can only by set on the server
	pd_private_rw: Readable only by the authenticated client it belongs to, set by the authenticated client it belongs to
	pd_public_ro: Readable by any client, can only be set on the server
	pd_public_rw: Readable by any client, set by the authenicated client is belongs to
	*********/
	typedef enum { pd_private_ro, pd_private_rw, pd_public_ro, pd_public_rw } persisttype_t;

	enum EPersistRequestType {
		EPersistRequestType_SetUserData,
		EPersistRequestType_GetUserData,
		EPersistRequestType_GetUserKeyedData,
		EPersistRequestType_NewGame,
		EPersistRequestType_UpdateGame,
		EPersistRequestType_GetGameInfoByGamename,
		EPersistRequestType_Auth_ProfileID,
		EPersistRequestType_Auth_AuthTicket,
		EPersistRequestType_Auth_CDKey,
		EPersistRequestType_GetProfileIDFromCDKey
	};

	typedef struct {
		std::string game_instance_identifier;
		uint32_t mod_time;
		OS::GameData gameData;

		OS::KVReader kv_data;
	} PersistBackendResponse;

	typedef void(*PersistBackendCallback)(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra);


	typedef struct {
		int type;

		GS::Peer *mp_peer;
		void* 	  mp_extra;
		PersistBackendCallback callback;

		std::map<std::string, std::string> kvMap;
		std::vector<std::string> keyList;
		std::string game_instance_identifier; //also used as auth "response" value
		std::string auth_token;

		std::string profile_nick;
		int profileid; //and gameid

		int modified_since; //also used as session key for auth

		persisttype_t data_type;
		int data_index;
		bool data_kv_set;

		bool complete;

		OS::KVReader kv_set_data;

		TaskShared::AuthCallback authCallback;
	} PersistBackendRequest;

    bool Perform_GetGameInfo(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_GetUserData(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_GetUserKeyedData(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_SetUserData(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_NewGame(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_UpdateGame(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_ProfileIDAuth(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_AuthTokenAuth(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_CDKeyAuth(PersistBackendRequest request, TaskThreadData *thread_data);
	bool Perform_GetProfileIDFromCD(PersistBackendRequest request, TaskThreadData *thread_data);

	void InitTasks();
	void PerformUVWorkRequest(uv_work_t *req);
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status);

	//void Handle_WebError(TaskShared::AuthData &data, json_t *error_obj);
}
#endif //_GS_TASKS_H