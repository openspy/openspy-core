#include "NNPeer.h"
#include "NNDriver.h"
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/helpers.h>

#include "structs.h"
#include "NNBackend.h"


namespace NN {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		m_sd = sd;
		m_address_info = *address_info;

		m_cookie = 0;
		m_client_index = 0;
		m_client_version = 0;
	}
	Peer::~Peer() {
		close(m_sd);
	}
	void Peer::think() {
		struct timeval time_now;
		gettimeofday(&time_now, NULL);

		if(time_now.tv_sec - m_last_recv.tv_sec > NN_TIMEOUT_TIME) {
			m_delete_flag = true;
			m_timeout_flag = true;
			return;
		}

		if(m_peer_address.GetIP() == 0) {
			if(time_now.tv_sec - m_init_time.tv_sec > NN_DEADBEAT_TIME) {
				sendPeerIsDeadbeat();
				m_delete_flag = true;
			}
		}
	}
	void Peer::handle_packet(char *recvbuf, int len) {
		NatNegPacket *packet = (NatNegPacket *)recvbuf;
		unsigned char NNMagicData[] = {NN_MAGIC_0, NN_MAGIC_1, NN_MAGIC_2, NN_MAGIC_3, NN_MAGIC_4, NN_MAGIC_5};
		if(memcmp(&NNMagicData,&packet->magic, NATNEG_MAGIC_LEN) != 0) {
			m_delete_flag = true;
			return;
		}

		gettimeofday(&m_last_recv, NULL);

		m_client_version = packet->version;

		switch(packet->packettype) {
			case NN_INIT:
				handleInitPacket(packet);
			break;
			case NN_ADDRESS_CHECK:
				handleAddressCheckPacket(packet);
			break;
			case NN_NATIFY_REQUEST:
				printf("NN::NatifyRequest\n");
			break;
			case NN_CONNECT_PING: //should never be recieved
			break;
			case NN_CONNECT_ACK:
			break;
			case NN_REPORT:
				handleReportPacket(packet);
			break;
			default:
				printf("unknown natneg packet: %d\n",packet->packettype);
			break;
		}
	}
	void Peer::handleInitPacket(NatNegPacket *packet) {
		m_cookie = packet->cookie;
		m_client_index = packet->Packet.Init.clientindex;

		NN::NNQueryTask::getQueryTask()->SubmitClient(this);
		
		packet->packettype = NN_INITACK;
		sendPacket(packet);

		gettimeofday(&m_init_time, NULL);
	}
	void Peer::handleReportPacket(NatNegPacket *packet) {
		packet->packettype = NN_REPORT_ACK;
		sendPacket(packet);
	}
	void Peer::handleAddressCheckPacket(NatNegPacket *packet) {
		packet->packettype = NN_ADDRESS_REPLY;
		packet->Packet.Init.localip = m_address_info.sin_addr.s_addr;
		packet->Packet.Init.localport = m_address_info.sin_port;
		sendPacket(packet);
	}
	void Peer::sendPacket(NatNegPacket *packet) {
		int size = 0;
		switch(packet->packettype) {
			case NN_ADDRESS_REPLY:
			case NN_INITACK:
				size = INITPACKET_SIZE;
				break;
			case NN_CONNECT:
				size = CONNECTPACKET_SIZE;
				break;
			case NN_REPORT_ACK:
				size = REPORTPACKET_SIZE;
				break;

		}
		sendto(m_sd,(char *)packet,size,0,(struct sockaddr *)&m_address_info,sizeof(struct sockaddr));
	}
	OS::Address Peer::getAddress() {
		return OS::Address(m_address_info);
	}
	void Peer::OnGotPeerAddress(OS::Address address) {
		if(m_peer_address.GetIP() == 0) {
			m_peer_address = address;
			NN::NNQueryTask::getQueryTask()->SubmitClient(this); //resubmit for other client
			SendConnectPacket(m_peer_address);
		}
	}
	void Peer::sendPeerIsDeadbeat() {
		NatNegPacket p;
		memcpy(p.magic, NNMagicData, NATNEG_MAGIC_LEN);
		p.version = m_client_version;
		p.packettype = NN_CONNECT;
		p.cookie = m_cookie;
		p.Packet.Connect.finished = FINISHED_ERROR_DEADBEAT_PARTNER;
		sendPacket(&p);
	}
	void Peer::SendConnectPacket(OS::Address address) {
		NatNegPacket p;
		struct sockaddr_in remote_addr = m_peer_address.GetInAddr();
		memcpy(p.magic, NNMagicData, NATNEG_MAGIC_LEN);

		p.version = m_client_version;
		p.Packet.Connect.gotyourdata = 'B'; //??
		p.packettype = NN_CONNECT;
		p.cookie = m_cookie;
		
		p.Packet.Connect.finished = FINISHED_NOERROR;

		p.Packet.Connect.remoteIP = remote_addr.sin_addr.s_addr;
		p.Packet.Connect.remotePort = remote_addr.sin_port;
		sendPacket(&p);
	}

}