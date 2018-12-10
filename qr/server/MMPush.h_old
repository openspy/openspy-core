#ifndef _MMPUSH_H
#define _MMPUSH_H
#include <OS/OpenSpy.h>
#include <map>
#include <vector>
#include <string>
#include <OS/Redis.h>

#include <OS/Task.h>
#include <OS/TaskPool.h>

#include "QRServer.h"

#include <OS/Timer/HiResTimer.h>

namespace QR {
	class Server;
	class Driver;
	class Peer;
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
		EMMPushRequestType type;
		QR::Peer *peer;
		ServerInfo server;
		ServerInfo old_server;
		std::string gamename;
		int state;
	};
	class MMPushTask : public OS::Task<MMPushRequest> {
		public:
			MMPushTask(int thread_index);
			~MMPushTask();

			void AddDriver(QR::Driver *driver);
			void RemoveDriver(QR::Driver *driver);
			int TryFindServerID(ServerInfo server);
			static void MQListenerCallback(std::string message, void *extra);
		private:
			static void *TaskThread(OS::CThread *thread);
			void PerformPushServer(MMPushRequest request);
			void PerformUpdateServer(MMPushRequest request);
			void PerformDeleteServer(MMPushRequest request);
			void PerformGetGameInfo(MMPushRequest request);

			//server update functions
			void PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request);
			
			int PushServer(ServerInfo server, bool publish, int pk_id = -1);
			void UpdateServer(ServerInfo server);
			void DeleteServer(ServerInfo server, bool publish);
			int GetServerID();
			std::vector<QR::Driver *> m_drivers;
			Redis::Connection *mp_redis_connection;
			time_t m_redis_timeout;

			bool m_thread_awake;
			OS::HiResTimer *mp_timer;

			int m_thread_index;


	};

	#define NUM_MM_PUSH_THREADS 8
	#define MM_WAIT_MAX_TIME 1500
	extern OS::TaskPool<MMPushTask, MMPushRequest> *m_task_pool;
	void SetupTaskPool(QR::Server *server);
	void *setup_redis_async(OS::CThread *thread);
}
#endif //_MMPUSH_H