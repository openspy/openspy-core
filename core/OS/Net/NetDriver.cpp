#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {
	m_server = server;
    mp_head = NULL;

}
INetDriver::~INetDriver() {

}
INetIOInterface<> *INetDriver::getNetIOInterface() {
	return mp_net_io_interface;
}
void INetDriver::setNetIOInterface(INetIOInterface<> *iface) {
	mp_net_io_interface = iface;
}
void INetDriver::addPeerToList(INetPeer* peer) {
    if (mp_head == NULL) {
        mp_head = peer;
    }
    else {
        INetPeer* p = mp_head;
        while (p->GetNext() != NULL) {
            p = p->GetNext();
        }
        p->SetNext(peer);
    }
}
