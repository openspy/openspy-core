#ifndef _BSDNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetIOInterface.h>
#include <vector>

class BSDNetIOInterface : public INetIOInterface {
	public:
		BSDNetIOInterface();
		~BSDNetIOInterface();
		//NET IO INTERFACE
		INetIOSocket *BindTCP(OS::Address bind_address);
		std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket);
		NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer);
		NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer);

		INetIOSocket *BindUDP(OS::Address bind_address);
		NetIOCommResp datagramRecv(INetIOSocket *socket, std::vector<INetIODatagram> &datagrams);
		NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer);
		void closeSocket(INetIOSocket *socket);
		void flushSendQueue();
		void flushSocketFromSendQueue(INetIOSocket *socket);
		void makeNonBlocking(int sd);
		//
	private:
		std::map<INetIOSocket *, std::vector<OS::Buffer> > m_datagram_send_queue;
		std::map<INetIOSocket *, std::vector<OS::Buffer> > m_stream_send_queue;
};

#endif //_BSDNETIOINTERFACE_H