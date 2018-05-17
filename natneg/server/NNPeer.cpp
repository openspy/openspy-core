#include "NNPeer.h"
#include "NNDriver.h"
#include <OS/legacy/helpers.h>

#include "structs.h"
#include "NNBackend.h"


namespace NN {
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;

		m_cookie = 0;
		m_client_index = -1;
		m_client_version = 0;
		m_gamename = "";
		m_found_partner = false;
		m_got_natify_request = false;
		m_got_init = false;
		m_got_preinit = false;
		m_sent_connect = false;
		memset(&m_ert_test_time, 0, sizeof(m_ert_test_time));
		memset(&m_init_time, 0, sizeof(m_init_time));
		ResetMetrics();
		m_peer_stats.m_address = sd->address;
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",sd->address.ToString().c_str());

	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
	}
	void Peer::think(bool waiting_packet) {
		struct timeval time_now;
		gettimeofday(&time_now, NULL);

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
		if(!m_found_partner) {
			if(time_now.tv_sec - m_init_time.tv_sec > NN_DEADBEAT_TIME && m_init_time.tv_sec != 0) {
				sendPeerInitError(FINISHED_ERROR_DEADBEAT_PARTNER);
				Delete();
			}
		}
		else {
			if (m_got_natify_request && time_now.tv_sec - m_ert_test_time.tv_sec > NN_NATIFY_WAIT_TIME) {
				if (m_found_partner) {
					if (m_got_preinit) {
						SendPreInitPacket(NN_PREINIT_READY);
					}
					else {
						SendConnectPacket(m_peer_address);
					}					
				}
			}
		}
	}
	void Peer::handle_packet(INetIODatagram net_packet) {
		m_peer_stats.packets_in++;
		m_peer_stats.bytes_in += net_packet.buffer.remaining();
		NatNegPacket *packet = (NatNegPacket *)net_packet.buffer.GetHead();
		unsigned char NNMagicData[] = {NN_MAGIC_0, NN_MAGIC_1, NN_MAGIC_2, NN_MAGIC_3, NN_MAGIC_4, NN_MAGIC_5};
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

		int len = net_packet.buffer.remaining();
		
		if (((char *)net_packet.buffer.GetHead())[len-1] == 0 && m_gamename.length() == 0 && m_client_version > 2) {
			m_gamename = (const char *)&((char *)net_packet.buffer.GetHead())[packetSize];
		}
		switch(packet->packettype) {
			case NN_PREINIT:
				m_sent_connect = false;
				handlePreInitPacket(packet);
			break;
			case NN_INIT:
				m_sent_connect = false;
				m_got_init = true;
				handleInitPacket(packet);
			break;
			case NN_ADDRESS_CHECK:
				handleAddressCheckPacket(packet);
			break;
			case NN_NATIFY_REQUEST:
				handleNatifyPacket(packet);
			break;
			case NN_CONNECT_PING:
			break;
			case NN_CONNECT_ACK:
				if (m_client_version <= 2) {
					Delete();
				}
			break;
			case NN_REPORT:
				handleReportPacket(packet);
				Delete();
			break;
			default:
				OS::LogText(OS::ELogLevel_Info, "[%s] Got unknown packet type: %d, version: %d", m_sd->address.ToString().c_str(), packet->packettype, packet->version);
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
		m_private_address = OS::Address(packet->Packet.Init.localip, packet->Packet.Init.localport);

		OS::LogText(OS::ELogLevel_Info, "[%s] Got init - version: %d, client idx: %d, cookie: %d, game: %s", m_sd->address.ToString().c_str(), packet->version, m_client_index, m_cookie, m_gamename.c_str());

		SubmitClient();

		
		packet->packettype = NN_INITACK;
		sendPacket(packet);

		if (m_found_partner) {
			SendConnectPacket(m_peer_address);
		}
		else {
			gettimeofday(&m_init_time, NULL);
		}
	}
	void Peer::handleNatifyPacket(NatNegPacket *packet) {
		NNBackendRequest req;
		if (!m_got_natify_request) {
			m_got_natify_request = true;
			gettimeofday(&m_ert_test_time, NULL);
		}

		OS::LogText(OS::ELogLevel_Info, "[%s] Got ERTTest type %d", m_sd->address.ToString().c_str(), packet->Packet.Init.porttype);
		switch (packet->Packet.Init.porttype) {
			//case NN_PT_GP: //??
			case NN_PT_NN1: //solicited ERT reply
				packet->packettype = NN_ERTTEST;
				sendPacket(packet);
				break;
			case NN_PT_NN2: //unsolicited IP ERT reply
				req.type = NN::ENNQueryRequestType_PerformERTTest;
				req.extra = (void *)2;
				this->IncRef();
				req.peer = this;
				NN::m_task_pool->AddRequest(req);
				break;
			case NN_PT_NN3: //unsolicited IP&Port ERT reply				
				req.type = NN::ENNQueryRequestType_PerformERTTest;
				req.extra = (void *)3;
				this->IncRef();
				req.peer = this;
				NN::m_task_pool->AddRequest(req);
				break;
		}
	}
	void Peer::handleReportPacket(NatNegPacket *packet) {
		OS::LogText(OS::ELogLevel_Info, "[%s] Got report- client idx: %d, cookie: %d, game: %s, port type: %d, neg result: %d, neg type: %d, nat mapping scheme: %d", m_sd->address.ToString().c_str(), m_client_index, m_cookie, packet->Packet.Report.gamename, packet->Packet.Report.porttype, packet->Packet.Report.negResult, packet->Packet.Report.natType, packet->Packet.Report.natMappingScheme);
		packet->packettype = NN_REPORT_ACK;
		sendPacket(packet);

	}
	void Peer::handleAddressCheckPacket(NatNegPacket *packet) {
		packet->packettype = NN_ADDRESS_REPLY;
		packet->Packet.Init.localip = m_sd->address.GetInAddr().sin_addr.s_addr;
		packet->Packet.Init.localport = m_sd->address.GetInAddr().sin_port;
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

		m_peer_stats.packets_out++;
		m_peer_stats.bytes_out += size;

		OS::Buffer buffer(size);
		buffer.WriteBuffer(packet, size);
		buffer.SetCursor(size);
		NetIOCommResp resp = GetDriver()->getServer()->getNetIOInterface()->datagramSend(m_sd, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			Delete();
		}
	}
	void Peer::OnGotPeerAddress(OS::Address address, OS::Address private_address) {
		if (m_found_partner) {
			return;
		}
		if (address.GetIP() == getAddress().GetIP()) {
			m_peer_address = private_address;
		}
		else {
			m_peer_address = address;
		}
		
		m_found_partner = true;

		/*if (is_preinit && m_got_preinit) {
			struct timeval time_now;
			gettimeofday(&time_now, NULL);
			if (time_now.tv_sec - m_ert_test_time.tv_sec > NN_NATIFY_WAIT_TIME) {
				printf("send preinit ready\n");
				SendPreInitPacket(NN_PREINIT_READY);
			}
			else {
				printf("send preinit matchup\n");
				SendPreInitPacket(NN_PREINIT_WAITING_FOR_MATCHUP);
				//SendPreInitPacket(NN_PREINIT_READY);
			}
		}
		else*/ {
			struct timeval time_now;
			gettimeofday(&time_now, NULL);
			if (!m_got_natify_request || time_now.tv_sec - m_ert_test_time.tv_sec > NN_NATIFY_WAIT_TIME) {
				SendConnectPacket(m_peer_address);
			}
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
		OS::LogText(OS::ELogLevel_Info, "[%s] Sending init timeout", m_sd->address.ToString().c_str());
		m_delete_flag = true;
	}
	void Peer::SendConnectPacket(OS::Address address) {
		if (!m_sent_connect) {
			m_sent_connect = true;
		}
		else {
			return;
		}
		NatNegPacket p;
		struct sockaddr_in remote_addr = address.GetInAddr();

		p.packettype = NN_CONNECT;
		p.Packet.Connect.finished = FINISHED_NOERROR;

		p.Packet.Connect.remoteIP = remote_addr.sin_addr.s_addr;
		p.Packet.Connect.remotePort = remote_addr.sin_port;
		p.Packet.Connect.gotyourdata = 1;

		sendPacket(&p);

		OS::LogText(OS::ELogLevel_Info, "[%s] Connect Packet (to: %s)", m_sd->address.ToString().c_str(), address.ToString().c_str());

		//p.Packet.Connect.gotyourdata = 1;
		//p.packettype = NN_CONNECT_PING;
		//sendPacket(&p);
	}
	void Peer::SendPreInitPacket(uint8_t state) {
		NatNegPacket p;

		p.version = m_client_version;

		p.Packet.Preinit.state = state;
		p.packettype = NN_PREINIT_ACK;
		sendPacket(&p);
	}
	void Peer::SubmitClient() {
		NNBackendRequest req;
		req.type = NN::ENNQueryRequestType_SubmitClient;
		this->IncRef();
		req.peer = this;
		m_peer_stats.pending_requests++;
		NN::m_task_pool->AddRequest(req);
	}
	OS::MetricInstance Peer::GetMetrics() {
		OS::MetricInstance peer_metric;

		peer_metric.value = GetMetricItemFromStats(m_peer_stats);
		peer_metric.key = "peer";

		ResetMetrics();

		return peer_metric;
	}

	OS::MetricValue Peer::GetMetricItemFromStats(PeerStats stats) {
		OS::MetricValue arr_value, value;
		value.value._str = stats.m_address.ToString(false);
		value.key = "ip";
		value.type = OS::MetricType_String;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.disconnected;
		value.key = "disconnected";
		value.type = OS::MetricType_Integer;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.value._int = stats.bytes_in;
		value.key = "bytes_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.bytes_out;
		value.key = "bytes_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_in;
		value.key = "packets_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_out;
		value.key = "packets_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.pending_requests;
		value.key = "pending_requests";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));
		arr_value.type = OS::MetricType_Array;

		value.type = OS::MetricType_String;
		value.key = "gamename";
		value.value._str = stats.from_game.gamename;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.key = "version";
		value.value._int = stats.version;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		arr_value.key = stats.m_address.ToString(false);
		return arr_value;
	}
	void Peer::ResetMetrics() {
		m_peer_stats.bytes_in = 0;
		m_peer_stats.bytes_out = 0;
		m_peer_stats.packets_in = 0;
		m_peer_stats.packets_out = 0;
		m_peer_stats.pending_requests = 0;
		m_peer_stats.from_game.gameid = 0;
		m_peer_stats.from_game.secretkey[0] = 0;
		m_peer_stats.from_game.gamename[0] = 0;
	}
}