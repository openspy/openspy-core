#ifndef _QRPEER_H
#define _QRPEER_H
#include "../main.h"

#include "QRDriver.h"

#include <OS/legacy/gsmsalg.h>

#include "MMPush.h"

#define REQUEST_KEY_LEN 4
#define CHALLENGE_LEN 64
namespace QR {
	class Driver;

	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		void handle_packet(char *recvbuf, int len);


		void SendPacket(const uint8_t *buff, int len);

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(uint8_t *data, int data_len);
	private:

		bool isTeamString(const char *string);

		void handle_heartbeat(char *buff, int len);
		void handle_challenge(char *buff, int len);
		void handle_keepalive(char *buff, int len);

		void send_challenge();
		Driver *mp_driver;


		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		int m_sd;

		bool m_recv_instance_key;
		uint8_t m_instance_key[REQUEST_KEY_LEN];
		char m_challenge[CHALLENGE_LEN + 1];
		bool m_sent_challenge;
		bool m_server_pushed;

		MM::ServerInfo m_server_info;
	};
}
#endif //_QRPEER_H