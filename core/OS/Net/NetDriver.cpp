#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {

    mp_peers = new OS::LinkedListHead<INetPeer*>();
	m_server = server;
    mp_net_io_interface = NULL;

}
INetDriver::~INetDriver() {

}
INetIOInterface<> *INetDriver::getNetIOInterface() {
	return mp_net_io_interface;
}
void INetDriver::setNetIOInterface(INetIOInterface<> *iface) {
	mp_net_io_interface = iface;
}