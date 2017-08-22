#ifndef _SBPEER_H
#define _SBPEER_H
#include "../main.h"
#include <OS/Ref.h>
#include "MMQuery.h"

//Maximum length for the SQL filter string
#define MAX_FILTER_LEN 511

#define MAX_FIELD_LIST_LEN 256

#define MAX_OUTGOING_REQUEST_SIZE (MAX_FIELD_LIST_LEN + MAX_FILTER_LEN + 255)


namespace SB {
	class Driver;
	class Server;

	struct sServerCache {
		std::string key;
		OS::Address wan_address;
		bool full_keys;
	};


	class Peer : public OS::Ref {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd, int version);
		virtual ~Peer();
		
		virtual void think(bool packet_waiting) = 0;
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };



		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		
		bool serverMatchesLastReq(MM::Server *server, bool require_push_flag = true);

		virtual void informDeleteServers(MM::Server *server) = 0;
		virtual void informNewServers(MM::Server *server) = 0;
		virtual void informUpdateServers(MM::Server *server) = 0;

		virtual void OnRetrievedServers(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRetrievedServerInfo(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRetrievedGroups(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) = 0;
		virtual void OnRecievedGameInfo(const OS::GameData game_data, void *extra) = 0;
		virtual void OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra) = 0;
	protected:
		void cacheServer(MM::Server *server, bool full_keys = false);
		void DeleteServerFromCacheByIP(OS::Address address);
		void DeleteServerFromCacheByKey(std::string key);
		sServerCache FindServerByIP(OS::Address address);
		sServerCache FindServerByKey(std::string key);

		int m_version;
		int m_sd;
		OS::GameData m_game;
		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		std::vector<sServerCache> m_visible_servers;

		MM::sServerListReq m_last_list_req;


		OS::CMutex *mp_mutex;
	private:




	};
}
#endif //_SAMPRAKPEER_H