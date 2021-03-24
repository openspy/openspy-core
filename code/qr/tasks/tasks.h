#ifndef _MM_TASKS_H
#define _MM_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>

#define NN_REDIS_EXPIRE_TIME 500
namespace QR {
    class Peer;
    class Server;
}
namespace MM {
	typedef struct {
		OS::GameData m_game;
		OS::Address  m_address;
		std::map<std::string, std::string> m_keys;
		std::map<std::string, std::vector<std::string> > m_player_keys;
		std::map<std::string, std::vector<std::string> > m_team_keys;

		int groupid;
		int id;
	} ServerInfo;

	void Shutdown();

	enum EMMPushRequestType {
		EMMPushRequestType_PushServer,
		EMMPushRequestType_UpdateServer,
		EMMPushRequestType_UpdateServer_NoDiff,
		EMMPushRequestType_DeleteServer,
		EMMPushRequestType_GetGameInfoByGameName,
	};
	class MMPushRequest {
	public:
		MMPushRequest() {
			type = EMMPushRequestType_PushServer;
			peer = NULL;
			state = 0;
		}
		~MMPushRequest() { }
		int type;
		QR::Peer *peer;
		ServerInfo server;
		ServerInfo old_server;
		std::string gamename;
		int state;
	};
    
    bool PerformPushServer(MMPushRequest request, TaskThreadData *thread_data);
    bool PerformUpdateServer(MMPushRequest request, TaskThreadData *thread_data);
    bool PerformDeleteServer(MMPushRequest request, TaskThreadData *thread_data);
    bool PerformGetGameInfo(MMPushRequest request, TaskThreadData *thread_data);

    //server update functions
    bool PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request, TaskThreadData *thread_data);

    bool Handle_ClientMessage(TaskThreadData *thread_data, std::string message);

    TaskScheduler<MMPushRequest, TaskThreadData> *InitTasks(INetServer *server);

	//shared functions
	void DeleteServer(TaskThreadData *thread_data, ServerInfo server, bool publish);
	void UpdateServer(TaskThreadData *thread_data, ServerInfo server);
	int GetServerID(TaskThreadData *thread_data);
	int PushServer(TaskThreadData *thread_data, ServerInfo server, bool publish, int pk_id = -1);
	int TryFindServerID(TaskThreadData *thread_data, ServerInfo server);

	extern const char *mm_channel_exchange;

	extern const char *mm_client_message_routingkey;
	extern const char *mm_server_event_routingkey;

    extern const char *mp_pk_name;

	//#define NUM_MM_PUSH_THREADS 8
	#define MM_WAIT_MAX_TIME 1500
	#define MM_PUSH_EXPIRE_TIME 1800
}
#endif //_MM_TASKS_H