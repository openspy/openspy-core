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
		
		virtual void think(bool packet_waiting) = 0;
		virtual void handle_packet(char *recvbuf, int len) = 0;		

		int GetSocket() { return m_sd; };
		const struct sockaddr_in *getAddress() { return &m_address_info; }
		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		
		virtual void send_error(bool die, const char *fmt, ...) = 0;
		virtual void SendClientMessage(uint8_t *data, int data_len) = 0;
	protected:

		bool isTeamString(const char *string);

		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		int m_sd;

		bool m_server_pushed;

		MM::ServerInfo m_server_info;
	};
}
#endif //_QRPEER_H