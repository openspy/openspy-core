#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "NNServer.h"

#include "NNDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
namespace NN {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		OS::Address bind_address(0, port);
		mp_net_io_interface = new BSDNetIOInterface<>();
		mp_socket = getNetIOInterface()->BindUDP(bind_address);

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
	}
	Driver::~Driver() {
		delete mp_mutex;
	}
	void Driver::think(bool listener_waiting) {
		mp_mutex->lock();
		if (listener_waiting) {
			std::vector<INetIODatagram> datagrams;
			getNetIOInterface()->datagramRecv(mp_socket, datagrams);
			std::vector<INetIODatagram>::iterator it = datagrams.begin();
			while (it != datagrams.end()) {
				INetIODatagram dgram = *it;
				if (dgram.comm_len == 0) {
					it++;
					continue;
				}
				if (dgram.error_flag) {
					//log error??
				}
				else {
					NatNegPacket packet;

					dgram.buffer.resetReadCursor();
					//PROCESS PACKET HERE
					do {
						void *begin = dgram.buffer.GetReadCursor();
						dgram.buffer.ReadBuffer(&packet.magic, NATNEG_MAGIC_LEN);
						if(memcmp(&NNMagicData,&packet.magic, NATNEG_MAGIC_LEN) != 0) {
							//skip invalid packet
							printf("INVALID PACKET\n");
							break;
						}
						packet.version = dgram.buffer.ReadByte();
						packet.packettype = dgram.buffer.ReadByte();
						packet.cookie = dgram.buffer.ReadInt();

						int packetSize = packetSizeFromType(packet.packettype);
						

						std::string gamename = "";
						size_t new_pos = 0;
						switch(packet.packettype) {
							case NN_INIT:
								dgram.buffer.ReadBuffer(&packet.Packet.Init, packetSize);
								gamename = dgram.buffer.ReadNTS();
								handle_init_packet(dgram.address, &packet, gamename);
							break;
							case NN_CONNECT_ACK:
								dgram.buffer.ReadBuffer(&packet.Packet.Init, packetSize);
								handle_connect_ack_packet(dgram.address, &packet, gamename);
							break;
							case NN_ADDRESS_CHECK:
								dgram.buffer.ReadBuffer(&packet.Packet.Init, packetSize);
								new_pos = ((size_t)begin-(size_t)dgram.buffer.GetHead());
								new_pos += sizeof(packet);
								dgram.buffer.SetReadCursor(new_pos);
								handle_address_check_packet(dgram.address, &packet, gamename);
							break;
							case NN_REPORT:
								dgram.buffer.ReadBuffer(&packet.Packet.Report, packetSize);
								handle_report_packet(dgram.address, &packet, gamename);
							break;
							case NN_NATIFY_REQUEST:
								dgram.buffer.ReadBuffer(&packet.Packet.Init, packetSize);
								new_pos = ((size_t)begin-(size_t)dgram.buffer.GetHead());
								new_pos += sizeof(packet);
								dgram.buffer.SetReadCursor(new_pos);
								handle_natify_packet(dgram.address, &packet, gamename);
							break;
							case NN_ERTACK:
								dgram.buffer.ReadBuffer(&packet.Packet.Init, packetSize);
								handle_ert_ack_packet(dgram.address, &packet, gamename);
							break;
						}

					} while(dgram.buffer.readRemaining() > 0);
				}
				it++;
			}
		}
		mp_mutex->unlock();
	}
	const std::vector<INetPeer *> Driver::getPeers(bool inc_ref) {
		return std::vector<INetPeer *>();
	}
	INetIOSocket *Driver::getListenerSocket() const {
		return mp_socket;
	}
	const std::vector<INetIOSocket *> Driver::getSockets() const {
		std::vector<INetIOSocket *> ret;
		ret.push_back(mp_socket);
		return ret;
	}
	void Driver::OnPeerMessage(INetPeer *peer) {

	}

	int Driver::packetSizeFromType(uint8_t type) {
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
		size -= BASEPACKET_SIZE;
		return size;
	}
	void Driver::SendPacket(OS::Address to,NatNegPacket *packet) {
		INetIOSocket client_socket = *mp_socket;
		client_socket.address = to;
		client_socket.shared_socket = true;

		int size = packetSizeFromType(packet->packettype) + BASEPACKET_SIZE;
		memcpy(&packet->magic, NNMagicData, NATNEG_MAGIC_LEN);

		OS::Buffer buffer(size);
		buffer.WriteBuffer(packet, size);

		NetIOCommResp resp = getNetIOInterface()->datagramSend(&client_socket, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d %d", to.ToString().c_str(), resp.disconnect_flag, resp.error_flag);
		}
	}
}
