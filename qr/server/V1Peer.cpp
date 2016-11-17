#include "V1Peer.h"
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
namespace QR {
		V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver,address_info, sd) {
			m_sd = sd;
			m_address_info = *address_info;
			m_server_pushed = false;
			m_delete_flag = false;
			m_timeout_flag = false;
		}
		V1Peer::~V1Peer() {

		}
		
		void V1Peer::send_ping() {

		}
		void V1Peer::think(bool packet_waiting) {
			printf("V1 think\n");
		} 

		void V1Peer::handle_packet(char *recvbuf, int len) {
			printf("QR1 handle packet %s\n", recvbuf);
		}


		void V1Peer::SendPacket(const uint8_t *buff, int len) {
			char sendbuf[MAX_DATA_SIZE + 1];
			uint8_t *p = (uint8_t *)&sendbuf;
			int blen = 0;
			BufferWriteData((uint8_t**)&p,&blen,buff, len);
			sendto(m_sd,(char *)&sendbuf,blen,0,(struct sockaddr *)&m_address_info, sizeof(sockaddr_in));
		}

		void V1Peer::send_error(const char *msg, bool die) {

		}
		void V1Peer::SendClientMessage(uint8_t *data, int data_len) {

		}
	
		bool V1Peer::isTeamString(const char *string) {

		}

		void V1Peer::handle_heartbeat(char *buff, int len) {
		}
}
	