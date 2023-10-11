#ifndef _NETDRIVER_H
#define _NETDRIVER_H
#include <OS/OpenSpy.h>
#include <OS/LinkedList.h>
#include "NetIOInterface.h"
#include <uv.h>
#include <stack>
class INetServer;
class INetPeer;
class INetDriver {
public:
	INetDriver(INetServer *server);
	virtual ~INetDriver();
	/*
		Check for incoming data, etc
	*/
	virtual void think(bool listen_waiting) = 0;
	INetServer *getServer() { return m_server; }
	virtual void OnPeerMessage(INetPeer *peer) = 0; //only used for "notify_driver_only registered peer" messages, currently incompatible with regular "registered peer"

	OS::LinkedListHead<INetPeer*>* GetPeerList() { return mp_peers; };


	void SendUDPPacket(OS::Address to, OS::Buffer buffer);
	static void on_udp_send_callback(uv_udp_send_t* req, int status);
protected:
	static void clear_send_buffer(uv_async_t *handle);
	INetServer *m_server;
	OS::LinkedListHead<INetPeer *>* mp_peers;

	uv_udp_t m_recv_udp_socket;
	uv_async_t m_udp_send_async_handler;
	std::stack<std::pair<OS::Address, OS::Buffer>> m_udp_send_buffer;
	struct sockaddr_in m_recv_addr;
	uv_mutex_t m_udp_send_mutex;
};
#endif //_NETDRIVER_H