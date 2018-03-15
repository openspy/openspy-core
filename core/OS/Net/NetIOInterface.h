#ifndef _NET_IO_INTERFACE_H
#define _NET_IO_INTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
typedef struct {
	int sd;
	OS::Address address;
} INetIOSocket;
class INetIOInterface {
	public:
		INetIOInterface() { };
		
		//socket setup
		virtual INetIOSocket *BindTCP(OS::Address bind_address) = 0;
		virtual std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket) = 0;
		virtual int streamRecv(INetIOSocket *socket, OS::Buffer &buffer) = 0;
		virtual int streamSend(INetIOSocket *socket, OS::Buffer &buffer) = 0;
		
		virtual INetIOSocket *BindUDP(OS::Address bind_address) = 0;
		virtual int datagramRecv(INetIOSocket *socket, OS::Buffer &buffer, OS::Address &data_source) = 0;
		virtual int datagramSend(INetIOSocket *socket, OS::Buffer &buffer, OS::Address &data_dest) = 0;

		virtual void closeSocket(INetIOSocket *socket) = 0;
		
};
#endif //_NET_IO_INTERFACE_H