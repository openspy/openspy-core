#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <openssl/ssl.h>

#define FESL_READ_SIZE                  (16 * 1024)


#define FESL_PING_TIME 120

typedef struct _FESL_HEADER {
	uint32_t type;
	uint32_t subtype;
	uint32_t len;
} FESL_HEADER;

enum {
	FESL_TYPE_FSYS = 0x73797366
};

namespace FESL {
	typedef struct _PeerStats {
		int total_requests;
		int version;

		long long bytes_in;
		long long bytes_out;

		int packets_in;
		int packets_out;

		OS::Address m_address;
		OS::GameData from_game;

		bool disconnected;
	} PeerStats;

	class Driver;
	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think(bool packet_waiting);
		void handle_packet(char *data, int len);
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void SendPacket(const uint8_t *buff, int len, bool attach_final = true);

		static OS::MetricValue GetMetricItemFromStats(PeerStats stats);
		OS::MetricInstance GetMetrics();
		PeerStats GetPeerStats() { if (m_delete_flag) m_peer_stats.disconnected = true; return m_peer_stats; };
	private:
		SSL *m_ssl_ctx;
		PeerStats m_peer_stats;
	
		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;
		void ResetMetrics();

	};
}
#endif //_GPPEER_H