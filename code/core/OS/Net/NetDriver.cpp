#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {
    mp_peers = new OS::LinkedListHead<INetPeer*>();
	m_server = server;
}
INetDriver::~INetDriver() {
	delete mp_peers;
}