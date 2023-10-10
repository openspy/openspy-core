#ifndef _NETDRIVER_H
#define _NETDRIVER_H
#include <OS/OpenSpy.h>
#include <OS/LinkedList.h>
#include "NetIOInterface.h"
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
protected:
	INetServer *m_server;
	OS::LinkedListHead<INetPeer *>* mp_peers;
};
#endif //_NETDRIVER_H