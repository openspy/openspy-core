#ifndef _MMPUSH_H
#define _MMPUSH_H
#include <OS/OpenSpy.h>
#include <map>
#include <vector>
#include <string>
namespace QR {
	class Driver;
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

	extern redisContext *mp_redis_connection;

	void Init(QR::Driver *driver);
	void PushServer(ServerInfo *server, bool publish = true);
	void UpdateServer(ServerInfo *server);
	void DeleteServer(ServerInfo *server, bool publish = true);

	int GetServerID();
}
#endif //_MMPUSH_H