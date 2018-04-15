#ifndef _NNPEER_H
#define _NNPEER_H
#include "../main.h"

#include "structs.h"

#include <OS/Net/NetPeer.h>

#define NN_DEADBEAT_TIME 60 //number of seconds for the natneg peer to wait before declaring the other party a no-show
#define NN_TIMEOUT_TIME 120 //time in seconds to d/c when haven't recieved any msg
#define NN_INIT_WAIT_TIME 30 //time in seconds to wait before d/cing when no init packet recieved
#define NN_NATIFY_WAIT_TIME 10 //wait 10 seconds for NN SDK to detect NAT type
namespace NN {

	class Driver;

	typedef struct _PeerStats {
		int pending_requests;
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
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();
		
		void think(bool waiting_packet);
		void handle_packet(INetIODatagram packet);

		OS::Address getPrivateAddress() { return m_private_address; };
		bool ShouldDelete() { return m_delete_flag; };
		void Delete(bool timeout = false) { m_delete_flag = true; m_timeout_flag = timeout; };
		bool IsTimeout() { return m_timeout_flag; }
		NNCookieType GetCookie() { return m_cookie; }
		uint8_t GetClientIndex() { return m_client_index; }

		void OnGotPeerAddress(OS::Address address, OS::Address private_address);
		std::string getGamename() { return m_gamename; };

		static OS::MetricValue GetMetricItemFromStats(PeerStats stats);
		OS::MetricInstance GetMetrics();
		PeerStats GetPeerStats() { if (m_delete_flag) m_peer_stats.disconnected = true; return m_peer_stats; };
	protected:
		void ResetMetrics();
		static int packetSizeFromType(uint8_t type);

		void SendConnectPacket(OS::Address address);
		void SendPreInitPacket(uint8_t state);
		void sendPeerInitError(uint8_t error);

		void handlePreInitPacket(NatNegPacket *packet);
		void handleInitPacket(NatNegPacket *packet);
		void handleReportPacket(NatNegPacket *packet);
		void handleAddressCheckPacket(NatNegPacket *packet);
		void handleNatifyPacket(NatNegPacket *packet);
		void SubmitClient();

		void sendPacket(NatNegPacket *packet);

		struct timeval m_last_recv, m_last_ping, m_init_time, m_ert_test_time;

		bool m_delete_flag;
		bool m_timeout_flag;
		bool m_got_init;

		NNCookieType m_cookie;
		uint8_t m_client_index;
		uint8_t m_client_version;
		uint8_t m_state;
		uint32_t m_client_id;
		OS::Address m_private_address;

		OS::Address m_peer_address;
		bool m_found_partner;

		std::string m_gamename;

		bool m_got_natify_request;
		bool m_got_preinit;
		bool m_sent_connect;

		PeerStats m_peer_stats;
	};
}
#endif //_QRPEER_H