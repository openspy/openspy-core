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
		EMMPushRequestType_DeleteServer,
	};
	typedef struct _MMPushRequest {
		EMMPushRequestType type;
		QR::Peer *peer;
		ServerInfo *server;
		void *extra;
	} MMPushRequest;
	class MMPushTask : public OS::Task<MMPushRequest> {
		public:
			MMPushTask();
			~MMPushTask();

			void AddDriver(QR::Driver *driver);
			void RemoveDriver(QR::Driver *driver);

			static void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);
		private:
			static void *TaskThread(OS::CThread *thread);
			void PerformPushServer(MMPushRequest request);
			void PerformUpdateServer(MMPushRequest request);
			void PerformDeleteServer(MMPushRequest request);

			void PushServer(ServerInfo *server, bool publish, int pk_id = -1);
			void UpdateServer(ServerInfo *server);
			void DeleteServer(ServerInfo *server, bool publish);
			int GetServerID();
			std::vector<QR::Driver *> m_drivers;
			Redis::Connection *mp_redis_connection;
			time_t m_redis_timeout;


	};

	#define NUM_MM_PUSH_THREADS 8
	extern OS::TaskPool<MMPushTask, MMPushRequest> *m_task_pool;
	void SetupTaskPool(QR::Server *server);
	void *setup_redis_async(OS::CThread *thread);
}
#endif //_MMPUSH_H