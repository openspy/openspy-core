#ifndef _NET_IO_INTERFACE_H
#define _NET_IO_INTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#ifdef _WIN32
typedef SOCKET socktype_t;
#else
typedef int socktype_t;
#endif
class INetIOSocket {
public:
	INetIOSocket() {
		sd = 0;
		shared_socket = false;
	}
	INetIOSocket(const INetIOSocket &socket) {
		shared_socket = true;
		sd = socket.sd;
		address = socket.address;
	}
	bool shared_socket;
	socktype_t sd;
	OS::Address address;
};
class NetIOCommResp {
public:
	NetIOCommResp() {
		error_flag = false;
		disconnect_flag = false;
		packet_count = 0;
		comm_len = 0;
	}
	NetIOCommResp(const NetIOCommResp &resp) {
		this->error_flag = resp.error_flag;
		this->comm_len = resp.comm_len;
		this->disconnect_flag = resp.disconnect_flag;
		this->packet_count = resp.packet_count;
	}
	bool error_flag;
	bool disconnect_flag;
	int packet_count;
	size_t comm_len;
};
class INetIODatagram {
public:
	INetIODatagram(const INetIODatagram &cpy) : buffer(cpy.buffer) {
		address = cpy.address;
		error_flag = cpy.error_flag;
		comm_len = cpy.comm_len;
	}
	INetIODatagram() {
		error_flag = false;
		comm_len = 0;
	}
	~INetIODatagram() {

	}
	bool operator==(const OS::Address &addr) const {
		return addr == address;
	}
	bool error_flag;
	OS::Address address;
	OS::Buffer buffer;
	int comm_len;
};
template<typename S = INetIOSocket>
class INetIOInterface {
	public:
		INetIOInterface() { };
		
		//socket setup
		virtual INetIOSocket *BindTCP(OS::Address bind_address) = 0;
		virtual std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket) = 0;
		virtual NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer) = 0;
		virtual NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer) = 0;
		
		virtual INetIOSocket *BindUDP(OS::Address bind_address) = 0;
		virtual NetIOCommResp datagramRecv(INetIOSocket *socket, std::vector<INetIODatagram> &datagrams) = 0;
		virtual NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer) = 0;

		virtual void closeSocket(INetIOSocket *socket) = 0;
		virtual void flushSendQueue() = 0;
		virtual void flushSocketFromSendQueue(INetIOSocket *socket) = 0;

		virtual S *createSocket() = 0;
};
#endif //_NET_IO_INTERFACE_H