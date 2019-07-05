#include "NNPeer.h"
#include "NNDriver.h"
#include "NNServer.h"

#include "structs.h"

#include "NATMapper.h"

#include <tasks/tasks.h>


namespace NN {
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;

		m_num_connect_resends = 0;

		m_cookie = 0;
		m_client_index = -1;
		m_client_version = 0;
		m_gamename = "";
		m_got_natify_request = false;
		m_got_init = false;
		m_got_preinit = false;
		m_use_gameport = false;
		m_got_connect_ack = false;
		memset(&m_ert_test_time, 0, sizeof(m_ert_test_time));
		memset(&m_init_time, 0, sizeof(m_init_time));
		memset(&m_last_connect_attempt, 0, sizeof(m_last_connect_attempt));
		

	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",getAddress().ToString().c_str());
	}
	void Peer::think(bool waiting_packet) {
		if (m_delete_flag) return;
		struct timeval time_now;
		gettimeofday(&time_now, NULL);

		bool sent_connect = m_last_connect_attempt.tv_sec > 0;

		if(time_now.tv_sec - m_last_recv.tv_sec > NN_TIMEOUT_TIME) {
			Delete(true);
			return;
		}
		if (!m_got_init) {
			if (time_now.tv_sec - m_init_time.tv_sec > NN_INIT_WAIT_TIME && m_init_time.tv_sec != 0) {
				sendPeerInitError(FINISHED_ERROR_INIT_PACKETS_TIMEDOUT);
				Delete();
			}
		}
		if(!sent_connect) {
			if(time_now.tv_sec - m_init_time.tv_sec > NN_DEADBEAT_TIME && m_init_time.tv_sec != 0) {
				sendPeerInitError(FINISHED_ERROR_DEADBEAT_PARTNER);
				Delete();
			}
		}
		else {
			if (time_now.tv_sec - m_ert_test_time.tv_sec > NN_NATIFY_WAIT_TIME) {
				if (m_got_preinit) {
					SendPreInitPacket(NN_PREINIT_READY);
				}
				else if(!m_got_connect_ack && ((time_now.tv_sec - m_last_connect_attempt.tv_sec) > NN_CONNECT_RESEND_TIME)) {
					SendConnectPacket(m_peer_address);
				}					
			}
		}
	}
	void Peer::handle_packet(INetIODatagram net_packet) {
		NatNegPacket *packet = (NatNegPacket *)net_packet.buffer.GetHead();
		if(memcmp(&NNMagicData,&packet->magic, NATNEG_MAGIC_LEN) != 0) {
			Delete();
			return;
		}

		gettimeofday(&m_last_recv, NULL);

		packet->cookie = htonl(packet->cookie);
		if (m_cookie == 0) {
			m_cookie = packet->cookie;
		}
		m_client_version = packet->version;

		int packetSize = packetSizeFromType(packet->packettype);

		size_t len = net_packet.buffer.readRemaining();
		
		if (((char *)net_packet.buffer.GetHead())[len-1] == 0 && m_gamename.length() == 0) {
			m_gamename = (const char *)&((char *)net_packet.buffer.GetHead())[packetSize];
		}
		switch(packet->packettype) {
			case NN_PREINIT:
				handlePreInitPacket(packet);
			break;
			case NN_INIT:
				handleInitPacket(packet);
			break;
			case NN_ADDRESS_CHECK:
				handleAddressCheckPacket(packet);
			break;
			case NN_NATIFY_REQUEST:
				handleNatifyPacket(packet);
			break;
			case NN_CONNECT_ACK:
				OS::LogText(OS::ELogLevel_Info, "[%s] Got connection ACK", getAddress().ToString().c_str());
				m_got_connect_ack = true;
				if (m_client_version <= 2) {
					Delete();
				}
			break;
			case NN_REPORT:
				handleReportPacket(packet);
				Delete();
			break;
			default:
				OS::LogText(OS::ELogLevel_Info, "[%s] Got unknown packet type: %d, version: %d", getAddress().ToString().c_str(), packet->packettype, packet->version);
			break;
		}
	}
	void Peer::handlePreInitPacket(NatNegPacket *packet) {
		if (m_got_preinit)
			return;
		m_client_id = packet->Packet.Preinit.clientID;
		m_client_index = packet->Packet.Preinit.clientindex;
		//SendPreInitPacket(NN_PREINIT_WAITING_FOR_CLIENT);
		m_got_preinit = true;
		SendPreInitPacket(NN_PREINIT_READY);
		//SubmitClient(true);
	}
	void Peer::handleInitPacket(NatNegPacket *packet) {
		m_client_index = packet->Packet.Init.clientindex;
		m_port_type = packet->Packet.Init.porttype;
		m_use_gameport = packet->Packet.Init.usegameport;
		m_private_address = OS::Address(packet->Packet.Init.localip, ntohs(packet->Packet.Init.localport));

		OS::LogText(OS::ELogLevel_Info, "[%s] Got init - version: %d, client idx: %d, cookie: %d, porttype: %d, use_gameport: %d, private: %s, game: %s", getAddress().ToString().c_str(), packet->version, m_client_index, m_cookie, m_port_type, m_use_gameport, m_private_address.ToString().c_str(), m_gamename.c_str());

		if (!m_got_init) {
			SubmitClient();
			m_got_init = true;
		}

		packet->packettype = NN_INITACK;
		sendPacket(packet);

		if (m_last_connect_attempt.tv_sec > 0 && !m_got_connect_ack) {
			SendConnectPacket(m_peer_address);
		}
		else {
			gettimeofday(&m_init_time, NULL);
		}
	}
	void Peer::handleNatifyPacket(NatNegPacket *packet) {
		//void AddRequest(TaskSchedulerRequestType type, ReqClass request);
		NNRequestData req;
		if (!m_got_natify_request) {
			m_got_natify_request = true;
			gettimeofday(&m_ert_test_time, NULL);
		}

		TaskScheduler<NNRequestData, TaskThreadData> *scheduler = ((NN::Server *)(GetDriver()->getServer()))->getScheduler();

		OS::LogText(OS::ELogLevel_Info, "[%s] Got ERTTest type %d", getAddress().ToString().c_str(), packet->Packet.Init.porttype);
		switch (packet->Packet.Init.porttype) {
			//case NN_PT_GP: //??
			case NN_PT_NN1: //solicited ERT reply
				packet->packettype = NN_ERTTEST;
				sendPacket(packet);
				break;
			case NN_PT_NN2: //unsolicited IP ERT reply
				this->IncRef();
				req.peer = this;
				scheduler->AddRequest(ENNRequestType_PerformERTTest_IPUnsolicited, req);
				break;
			case NN_PT_NN3: //unsolicited IP&Port ERT reply				
				this->IncRef();
				req.peer = this;
				scheduler->AddRequest(ENNRequestType_PerformERTTest_IPPortUnsolicited, req);
				break;
		}
	}
	void Peer::handleReportPacket(NatNegPacket *packet) {
		OS::LogText(OS::ELogLevel_Info, "[%s] Got report- client idx: %d, cookie: %d, game: %s, port type: %d, neg result: %d, neg type: %d, nat mapping scheme: %d", getAddress().ToString().c_str(), m_client_index, m_cookie, packet->Packet.Report.gamename, packet->Packet.Report.porttype, packet->Packet.Report.negResult, packet->Packet.Report.natType, packet->Packet.Report.natMappingScheme);
		packet->packettype = NN_REPORT_ACK;
		sendPacket(packet);

	}
	void Peer::handleAddressCheckPacket(NatNegPacket *packet) {
		packet->packettype = NN_ADDRESS_REPLY;
		packet->Packet.Init.localip = getAddress().GetInAddr().sin_addr.s_addr;
		packet->Packet.Init.localport = getAddress().GetInAddr().sin_port;
		sendPacket(packet);
	}
	int Peer::packetSizeFromType(uint8_t type) {
		int size = 0;
		switch (type) {
			case NN_PREINIT:
			case NN_PREINIT_ACK:
				size = PREINITPACKET_SIZE;
				break;
			case NN_ADDRESS_REPLY:
			case NN_NATIFY_REQUEST:
			case NN_ERTTEST:
			case NN_INIT:
			case NN_INITACK:
				size = INITPACKET_SIZE;
				break;
			case NN_CONNECT_ACK:
			case NN_CONNECT_PING:
			case NN_CONNECT:
				size = CONNECTPACKET_SIZE;
				break;
			case NN_REPORT:
			case NN_REPORT_ACK:
				size = REPORTPACKET_SIZE;
				break;

		}
		return size;
	}
	void Peer::sendPacket(NatNegPacket *packet) {
		int size = packetSizeFromType(packet->packettype);
		memcpy(&packet->magic, NNMagicData, NATNEG_MAGIC_LEN);

		packet->version = m_client_version;
		packet->cookie = htonl(m_cookie);

		OS::Buffer buffer(size);
		buffer.WriteBuffer(packet, size);

		NetIOCommResp resp = GetDriver()->getNetIOInterface()->datagramSend(m_sd, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			Delete();
		}
	}
	void Peer::OnGotPeerAddress(OS::Address public_address, OS::Address private_address, NN::NAT nat) {
		if (m_port_type == 0) { //stop NN V2(and maybe above?) bug - can't recv on game port
			Delete();
			return;
		}
		if (m_last_connect_attempt.tv_sec > 0) {
			return;
		}
		if (public_address.GetIP() == getAddress().GetIP()) {
			m_peer_address = private_address;
		}
		else {
			m_peer_address = public_address;
		}

		m_nat = nat;

		struct timeval time_now;
		gettimeofday(&time_now, NULL);
		if (!m_got_natify_request || time_now.tv_sec - m_ert_test_time.tv_sec > NN_NATIFY_WAIT_TIME) {
			SendConnectPacket(m_peer_address);
		}

		SubmitClient(); //resubmit for other client
	}
	void Peer::sendPeerInitError(uint8_t error) {
		NatNegPacket p;
		p.version = m_client_version;
		p.packettype = NN_CONNECT;
		p.Packet.Connect.remoteIP = 0;
		p.Packet.Connect.remotePort = 0;
		p.Packet.Connect.finished = error;
		sendPacket(&p);
		OS::LogText(OS::ELogLevel_Info, "[%s] Sending init timeout", getAddress().ToString().c_str());
	}
	void Peer::SendConnectPacket(OS::Address address) {
		gettimeofday(&m_last_connect_attempt, NULL);

		NatNegPacket p;
		struct sockaddr_in remote_addr = address.GetInAddr();

		p.packettype = NN_CONNECT;
		p.Packet.Connect.finished = FINISHED_NOERROR;

		p.Packet.Connect.remoteIP = remote_addr.sin_addr.s_addr;
		p.Packet.Connect.remotePort = remote_addr.sin_port;
		p.Packet.Connect.gotyourdata = 0;

		sendPacket(&p);

		OS::LogText(OS::ELogLevel_Info, "[%s] Connect Packet (to: %s), NAT mapping scheme: %s", getAddress().ToString().c_str(), address.ToString().c_str(), NN::GetNatMappingSchemeString(m_nat));

		if (m_num_connect_resends++ > NN_CONNECT_MAX_RESENDS) {
			Delete();
		}
	}
	void Peer::SendPreInitPacket(uint8_t state) {
		NatNegPacket p;

		p.version = m_client_version;

		p.Packet.Preinit.state = state;
		p.packettype = NN_PREINIT_ACK;
		sendPacket(&p);
	}
	void Peer::SubmitClient() {
		TaskScheduler<NNRequestData, TaskThreadData> *scheduler = ((NN::Server *)(GetDriver()->getServer()))->getScheduler();
		NNRequestData req;
		this->IncRef();
		req.peer = this;
		req.summary = GetSummary();
		scheduler->AddRequest(ENNRequestType_SubmitClient, req);
	}

	int Peer::NumRequiredAddresses() const {
		int required_addresses = 0;
		if (m_use_gameport) {
			required_addresses++;
		}
		if (m_client_version >= 2) {
			required_addresses += 2;
		}
		if (m_client_version >= 3) {
			required_addresses++;
		}
		return required_addresses;
	}
	NN::ConnectionSummary Peer::GetSummary() const {
		ConnectionSummary summary;
		summary.cookie = m_cookie;
		summary.index = m_client_index;
		summary.usegameport = m_use_gameport;
		summary.port_type = m_port_type;
		summary.private_address = m_private_address;
		if (m_use_gameport) {
			summary.gameport = m_private_address.GetPort();
		}
		return summary;
	}
}