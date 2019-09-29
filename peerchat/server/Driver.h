#ifndef _PEERCHATDRIVER_H
#define _PEERCHATDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/drivers/TCPDriver.h>

#include "Peer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace Peerchat {
	class Peer;
	class Driver;
	class Driver : public TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders = false);
		Peer *FindPeerByProfileID(int profileid);		
		Peer *FindPeerByUserSummary(std::string summary_string);
		void SendUserMessageToVisibleUsers(std::string fromSummary, std::string messageType, std::string message, bool includeSelf = true);
		void OnChannelMessage(std::string type, std::string from, std::string to, std::string message);
	protected:
		virtual INetPeer *CreatePeer(INetIOSocket *socket);
	};
}
#endif //_SBDRIVER_H