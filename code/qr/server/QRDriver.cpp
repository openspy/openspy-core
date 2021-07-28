#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"
#include "QRDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>

#include "v2.h"

namespace QR {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		OS::Address bind_address(0, port);
		mp_net_io_interface = new BSDNetIOInterface<>();
		mp_socket = getNetIOInterface()->BindUDP(bind_address);

		gettimeofday(&m_server_start, NULL);

	}
	Driver::~Driver() {
		getNetIOInterface()->closeSocket(mp_socket);
	}
	void Driver::think(bool listener_waiting) {
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

					char firstChar = dgram.buffer.ReadByte();
					dgram.buffer.resetReadCursor();

					int last_read_remaining = 0;
					
					//PROCESS PACKET HERE
					do {
						last_read_remaining = dgram.buffer.readRemaining();
						if(firstChar == '\\') { //v1

						} else { //v2
							handle_v2_packet(dgram);
						}

					} while(dgram.buffer.readRemaining() > 0 /*&& last_read_remaining != dgram.buffer.readRemaining()*/);
				}
				it++;
			}
		}
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
	void Driver::SendPacket(OS::Address to, OS::Buffer buffer) {
		INetIOSocket client_socket = *mp_socket;
		client_socket.address = to;
		client_socket.shared_socket = true;

		NetIOCommResp resp = getNetIOInterface()->datagramSend(&client_socket, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d %d", to.ToString().c_str(), resp.disconnect_flag, resp.error_flag);
		}
	}

	void Driver::send_v2_error(OS::Address to, uint32_t instance_key, uint8_t error_code, const char *error_message) {
		OS::Buffer buffer;		
		buffer.WriteByte(QR_MAGIC_1);
		buffer.WriteByte(QR_MAGIC_2);
		buffer.WriteByte(PACKET_ADDERROR);
		buffer.WriteInt(instance_key);
		buffer.WriteByte(error_code); //error code
		buffer.WriteNTS(error_message);
		SendPacket(to, buffer);
	}
}