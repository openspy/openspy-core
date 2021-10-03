#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "CDKeyServer.h"

#include <OS/gamespy/gamespy.h>

#include "CDKeyDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include <OS/KVReader.h>

namespace CDKey {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		OS::Address bind_address(0, port);
		mp_net_io_interface = new BSDNetIOInterface<>();
		mp_socket = getNetIOInterface()->BindUDP(bind_address);

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
	}
	Driver::~Driver() {
		getNetIOInterface()->closeSocket(mp_socket);
		delete mp_net_io_interface;
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
					dgram.buffer.resetReadCursor();

					gamespyxor((char *)dgram.buffer.GetHead(), dgram.buffer.bytesWritten());
					std::string input = std::string((const char *)dgram.buffer.GetHead(), dgram.buffer.bytesWritten());
					OS::KVReader data_parser = input;
					std::string command = data_parser.GetKeyByIdx(0);
					OS::LogText(OS::ELogLevel_Info, "[%s] Got key request: %s", dgram.address.ToString().c_str(), input.c_str());
					if(command.compare("auth") == 0) {
						handle_auth_packet(dgram.address, data_parser);
					}
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
	void Driver::SendPacket(OS::Address to, std::string message) {
		INetIOSocket client_socket = *mp_socket;
		client_socket.address = to;
		client_socket.shared_socket = true;

		OS::Buffer buffer;
		buffer.WriteBuffer(message.c_str(), message.length());
		gamespyxor((char *)buffer.GetHead(), buffer.bytesWritten());

		NetIOCommResp resp = getNetIOInterface()->datagramSend(&client_socket, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d %d", to.ToString().c_str(), resp.disconnect_flag, resp.error_flag);
		}
	}
}
