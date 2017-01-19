#ifndef _QRPEER_H
#define _QRPEER_H
#include "../main.h"

#include "structs.h"
#include "NNDriver.h"

#define NN_DEADBEAT_TIME 60 //number of seconds for the natneg peer to wait before declaring the other party a no-show
#define NN_TIMEOUT_TIME 120 //time in seconds to d/c when haven't recieved any msg
namespace NN {
	class Driver;

	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void think();
		void handle_packet(char *recvbuf, int len);		

		int GetSocket() { return m_sd; };
		OS::Address getAddress();
		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		uint32_t GetCookie() { return m_cookie; }
		uint8_t GetClientIndex() { return m_client_index; }

		void OnGotPeerAddress(OS::Address address);
	protected:

		void SendConnectPacket(OS::Address address);
		void sendPeerIsDeadbeat();

		void handleInitPacket(NatNegPacket *packet);
		void handleReportPacket(NatNegPacket *packet);
		void handleAddressCheckPacket(NatNegPacket *packet);


		void sendPacket(NatNegPacket *packet);

		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping, m_init_time;

		bool m_delete_flag;
		bool m_timeout_flag;

		int m_sd;

		uint32_t m_cookie;
		uint8_t m_client_index;
		uint8_t m_client_version;

		OS::Address m_peer_address;

	};
}
#endif //_QRPEER_H