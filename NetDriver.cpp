#include "NetDriver.h"
INetDriver::INetDriver(INetServer *server) {
	m_server = server;

}
INetDriver::~INetDriver() {

}