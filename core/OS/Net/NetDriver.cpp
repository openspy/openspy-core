#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {
	m_server = server;

}
INetDriver::~INetDriver() {

}