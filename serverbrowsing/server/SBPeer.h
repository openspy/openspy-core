#ifndef _SBPEER_H
#define _SBPEER_H
#include "../main.h"
#include <OS/Net/NetPeer.h>

#include "MMQuery.h"

//Maximum length for the SQL filter string
#define MAX_FILTER_LEN 511

#define MAX_FIELD_LIST_LEN 256

#define READ_BUFFER_SIZE 4096


namespace SB {
	class Driver;
	class Server;

	struct sServerCache {
		std::string key;
		OS::Address wan_address;
		bool full_keys;
	};

	typedef struct _PeerStats {
		int total_requests; //should be renamed to "pending requests"
		int version;

		long long bytes_in;
		long long bytes_out;

		int packets_in;
		int packets_out;

		OS::Address m_address;
		OS::GameData from_game;

		bool disconnected;
	} PeerStats;


	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd, int version);
		virtual ~Peer();
		
		virtual void think(bool packet_waiting) = 0;


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

		static OS::MetricValue GetMetricItemFromStats(PeerStats stats);
		OS::MetricInstance GetMetrics();
		PeerStats GetPeerStats() { if(m_delete_flag) m_peer_stats.disconnected = true; return m_peer_stats; };

		void Delete(bool timeout = false);
	protected:
		void cacheServer(MM::Server *server, bool full_keys = false);
		void DeleteServerFromCacheByIP(OS::Address address);
		void DeleteServerFromCacheByKey(std::string key);
		sServerCache FindServerByIP(OS::Address address);
		sServerCache FindServerByKey(std::string key);

		void ResetMetrics();

		int m_version;

		OS::GameData m_game;
		OS::Buffer m_recv_buffer;
		Driver *mp_driver;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		std::vector<sServerCache> m_visible_servers;
		MM::sServerListReq m_last_list_req;

		void AddRequest(MM::MMQueryRequest req);
		void FlushPendingRequests();
		std::queue<MM::MMQueryRequest> m_pending_request_list; //process after we retrieve src/dst gamenames


		OS::CMutex *mp_mutex;
		PeerStats m_peer_stats;
	private:




	};
}
#endif //_SAMPRAKPEER_H