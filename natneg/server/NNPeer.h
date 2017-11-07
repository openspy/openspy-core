#ifndef _QRPEER_H
#define _QRPEER_H
#include "../main.h"

#include "structs.h"
#include "NNDriver.h"

#include <OS/Net/NetPeer.h>

#define NN_DEADBEAT_TIME 60 //number of seconds for the natneg peer to wait before declaring the other party a no-show
#define NN_TIMEOUT_TIME 120 //time in seconds to d/c when haven't recieved any msg
#define NN_NATIFY_WAIT_TIME 10 //wait 10 seconds for NN SDK to detect NAT type
namespace NN {
	class Driver;

	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think(bool waiting_packet);
		void handle_packet(char *recvbuf, int len);		

		int GetSocket() { return m_sd; };
		OS::Address getAddress();
		bool ShouldDelete() { return m_delete_flag; };
		void SetDelete(bool set) { m_delete_flag = set; };
		bool IsTimeout() { return m_timeout_flag; }
		NNCookieType GetCookie() { return m_cookie; }
		uint8_t GetClientIndex() { return m_client_index; }

		void OnGotPeerAddress(OS::Address address);
		OS::MetricInstance GetMetrics();
		std::string getGamename() { return m_gamename; };
	protected:

		static int packetSizeFromType(uint8_t type);

		void SendConnectPacket(OS::Address address);
		void SendPreInitPacket(uint8_t state);
		void sendPeerIsDeadbeat();

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

		int m_sd;

		NNCookieType m_cookie;
		uint8_t m_client_index;
		uint8_t m_client_version;
		uint8_t m_state;
		uint32_t m_client_id;

		OS::Address m_peer_address;
		bool m_found_partner;

		std::string m_gamename;

		bool m_got_natify_request;
		bool m_got_preinit;
		bool m_sent_connect;
	};
}
#endif //_QRPEER_H