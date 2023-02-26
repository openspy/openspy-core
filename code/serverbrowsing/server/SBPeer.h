#ifndef _SBPEER_H
#define _SBPEER_H
#include "../main.h"
#include <OS/Net/NetPeer.h>

#include <tasks/tasks.h>

//Maximum length for the SQL filter string
#define MAX_FILTER_LEN 511

#define MAX_FIELD_LIST_LEN 256

#define READ_BUFFER_SIZE 4096

class CToken;

namespace SB {
	class Driver;
	class Server;

	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd, int version);
		virtual ~Peer();
		
		virtual void OnConnectionReady();
		
		virtual void think(bool packet_waiting) = 0;


		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		
		bool serverMatchesLastReq(MM::Server *server, bool require_push_flag = true);

		virtual void informDeleteServers(MM::Server *server) = 0;
		virtual void informNewServers(MM::Server *server) = 0;
		virtual void informUpdateServers(MM::Server *server) = 0;

		virtual void OnRetrievedServers(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRetrievedServerInfo(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRetrievedGroups(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRecievedGameInfo(const OS::GameData game_data, void *extra) = 0;
		virtual void OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra) = 0;

		virtual void Delete(bool timeout = false);
	protected:
		int m_version;

		OS::GameData m_game;
		Driver *mp_driver;

		struct timeval m_last_recv, m_last_ping;

		std::vector<CToken> m_last_list_req_token_list;
		MM::sServerListReq m_last_list_req;

		void AddRequest(MM::MMQueryRequest req);
		void FlushPendingRequests();
		std::queue<MM::MMQueryRequest> m_pending_request_list; //process after we retrieve src/dst gamenames


		OS::CMutex *mp_mutex;
	private:




	};
}
#endif //_SAMPRAKPEER_H