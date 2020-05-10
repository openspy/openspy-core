#ifndef _NETDRIVER_H
#define _NETDRIVER_H
#include <OS/OpenSpy.h>
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
	virtual INetIOSocket *getListenerSocket() const = 0;
	INetServer *getServer() { return m_server; }
	virtual void OnPeerMessage(INetPeer *peer) = 0; //only used for "notify_driver_only registered peer" messages, currently incompatible with regular "registered peer"
	INetIOInterface<> *getNetIOInterface();
	void setNetIOInterface(INetIOInterface<> *iface);
	void addPeerToList(INetPeer* peer);
	INetPeer* GetHead() { return mp_head; };
protected:
	INetServer *m_server;
	INetIOInterface<> *mp_net_io_interface;
	INetPeer* mp_head;
};
#endif //_NETDRIVER_H