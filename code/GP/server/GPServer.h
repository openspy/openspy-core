#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include <tasks/tasks.h>
#include <OS/Net/NetServer.h>

namespace GP {
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void tick();
		void shutdown();
		INetPeer *findPeerByProfile(int profile_id, bool inc_ref = true);
		void InformStatusUpdate(int from_profileid, GPShared::GPStatus status);
	private:
	};
}
#endif //_GPSERVER_H