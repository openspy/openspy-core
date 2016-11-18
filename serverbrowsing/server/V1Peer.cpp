#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <sstream>
#include "SBDriver.h"
#include "SBPeer.h"
#include "V1Peer.h"

namespace SB {
		V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : SB::Peer(driver, address_info, sd) {
			printf("V1 peer\n");
		}
		V1Peer::~V1Peer() {

		}
		
		void V1Peer::send_ping() {
			//check for timeout
			struct timeval current_time;
			
			uint8_t buff[10];
			uint8_t *p = (uint8_t *)&buff;
			int len = 0;

			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > SB_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);
				//SendPacket((uint8_t *)&buff, len, true);
			}
		}
		void V1Peer::think(bool packet_waiting) {
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			socklen_t slen = sizeof(struct sockaddr_in);
			int len;
			if (packet_waiting) {
				len = recv(m_sd, (char *)&buf, sizeof(buf), 0);
				this->handle_packet(buf, len);
			}
			
			send_ping();

			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_recv.tv_sec > SB_PING_TIME*2) {
				m_delete_flag = true;
				m_timeout_flag = true;
			}
		}

		void V1Peer::informDeleteServers(MM::ServerListQuery servers) {

		}
		void V1Peer::informNewServers(MM::ServerListQuery servers) {

		}
		void V1Peer::informUpdateServers(MM::ServerListQuery servers) {

		}
		void V1Peer::handle_packet(char *data, int len) {
			printf("SB V1: %s\n",data);
		}
		void V1Peer::SendPacket(uint8_t *buff, int len, bool prepend_length) {
			uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
			uint8_t *p = (uint8_t*)&out_buff;
			int out_len = 0;
			int header_len = 0;
			BufferWriteData(&p, &out_len, buff, len);
			if(send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL) < 0)
				m_delete_flag = true;
		}
}