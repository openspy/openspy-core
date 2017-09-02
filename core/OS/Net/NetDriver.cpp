#include "NetDriver.h"
#include "NetPeer.h"

#ifndef _WIN32
#include <sys/ioctl.h>
#endif
INetDriver::INetDriver(INetServer *server) {
	m_server = server;

}
INetDriver::~INetDriver() {

}

void INetDriver::makeNonBlocking(int sd) {
	unsigned long mode = 1;
	#ifdef _WIN32
	ioctlsocket(sd, FIONBIO, &mode);
	#elif defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (mode = fcntl(sd, F_GETFL, 0)))
		mode = 0;
	fcntl(sd, F_SETFL, flags | O_NONBLOCK);
	#else
	/* Otherwise, use the old way of doing it */
	ioctl(sd, FIONBIO, &mode);
	#endif
}
void INetDriver::makeNonBlocking(INetPeer *peer) {
	makeNonBlocking(peer->GetSocket());
}