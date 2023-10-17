#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "NNServer.h"

#include "NNDriver.h"
namespace NN {
	void alloc_buffer(uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t* buf) {
		buf->base = (char *)malloc(suggested_size);
		buf->len = suggested_size;
	}
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		uv_udp_init(uv_default_loop(), &m_recv_udp_socket);
		uv_handle_set_data((uv_handle_t*) &m_recv_udp_socket, this);
		
    	uv_ip4_addr(host, port, &m_recv_addr);

    	uv_udp_bind(&m_recv_udp_socket, (const struct sockaddr *)&m_recv_addr, UV_UDP_REUSEADDR);
    	uv_udp_recv_start(&m_recv_udp_socket, alloc_buffer, Driver::on_udp_read);

		gettimeofday(&m_server_start, NULL);
	}
	Driver::~Driver() {
		uv_close((uv_handle_t *)&m_recv_udp_socket, NULL); //XXX: this should actually be moved to not be in the ctor..
	}

	void Driver::on_udp_read(uv_udp_t* handle,
                               ssize_t nread,
                               const uv_buf_t* buf,
                               const struct sockaddr* addr,
                               unsigned flags) {
		Driver *driver = (Driver *)uv_handle_get_data((uv_handle_t*) handle);
		if(nread > 0) {
			OS::Buffer buffer;
			buffer.WriteBuffer(buf->base, nread);
			buffer.resetReadCursor();

			OS::Address address = *(struct sockaddr_in *)addr;

			NatNegPacket packet;

			void *begin = buffer.GetReadCursor();
			buffer.ReadBuffer(&packet.magic, NATNEG_MAGIC_LEN);
			if(memcmp(&NNMagicData,&packet.magic, NATNEG_MAGIC_LEN) != 0) {
				//skip invalid packet
				return;
			}
			memset(&packet.Packet.Init, 0, sizeof(packet.Packet.Init));
			packet.version = buffer.ReadByte();
			packet.packettype = buffer.ReadByte();
			packet.cookie = buffer.ReadInt();

			int packetSize = packetSizeFromType(packet.packettype, packet.version);
			std::string gamename;

			
			size_t new_pos = 0;
			switch(packet.packettype) {
				case NN_INIT:
					buffer.ReadBuffer(&packet.Packet.Init, packetSize);
					if(packet.version > 1) {
						gamename = buffer.ReadNTS();
					}								
					driver->handle_init_packet(address, &packet, gamename);
				break;
				case NN_CONNECT_ACK:
					buffer.ReadBuffer(&packet.Packet.Init, packetSize);
					driver->handle_connect_ack_packet(address, &packet);
				break;
				case NN_ADDRESS_CHECK:
					buffer.ReadBuffer(&packet.Packet.Init, packetSize);
					new_pos = ((size_t)begin-(size_t)buffer.GetHead());
					new_pos += sizeof(packet);
					buffer.SetReadCursor(new_pos);
					driver->handle_address_check_packet(address, &packet);
				break;
				case NN_REPORT:
					buffer.ReadBuffer(&packet.Packet.Report, packetSize);
					driver->handle_report_packet(address, &packet);
				break;
				case NN_NATIFY_REQUEST:
					buffer.ReadBuffer(&packet.Packet.Init, packetSize);
					new_pos = ((size_t)begin-(size_t)buffer.GetHead());
					new_pos += sizeof(packet);
					buffer.SetReadCursor(new_pos);
					driver->handle_natify_packet(address, &packet);
				break;
				case NN_ERTACK:
					buffer.ReadBuffer(&packet.Packet.Init, packetSize);
					driver->handle_ert_ack_packet(address, &packet);
				break;
			}			
		}
	}

	void Driver::think(bool listener_waiting) {

	}
	const std::vector<INetPeer *> Driver::getPeers(bool inc_ref) {
		return std::vector<INetPeer *>();
	}


	int Driver::packetSizeFromType(uint8_t type, uint8_t version) {
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
			case NN_CONNECT_ACK:
				size = INITPACKET_SIZE;
				if(version == 1) {
					size -= sizeof(uint32_t) + sizeof(uint16_t); //missing localo ip + port
				}
				break;			
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
		int size = packetSizeFromType(packet->packettype, packet->version) + BASEPACKET_SIZE;
		memcpy(&packet->magic, NNMagicData, NATNEG_MAGIC_LEN);

		OS::Buffer buffer(size);
		buffer.WriteBuffer(packet, size);

		SendUDPPacket(to, buffer);
	}
	void Driver::AddRequest(NNRequestData req) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		NNRequestData *work_data = new NNRequestData();
		*work_data = req;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);

		uv_queue_work(uv_default_loop(), uv_req, PerformUVWorkRequest, PerformUVWorkRequestCleanup);
	}
}
