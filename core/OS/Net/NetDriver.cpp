#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {
	m_server = server;

}
INetDriver::~INetDriver() {

}
INetIOInterface<> *INetDriver::getNetIOInterface() {
	return mp_net_io_interface;
}
void INetDriver::setNetIOInterface(INetIOInterface<> *iface) {
	mp_net_io_interface = iface;
}
