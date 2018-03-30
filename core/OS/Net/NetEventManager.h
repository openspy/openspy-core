#ifndef _NETEVENTMGR_H
#define _NETEVENTMGR_H
#include "NetDriver.h"
#include <vector>
class INetEventManager {
	public:
		INetEventManager();
		virtual ~INetEventManager();
		virtual void run() = 0;
		void addNetworkDriver(INetDriver *driver);

		virtual void RegisterSocket(INetPeer *peer) = 0;
		virtual void UnregisterSocket(INetPeer *peer) = 0;
	protected:
		std::vector<INetDriver *> m_net_drivers;
};
#endif //_NETEVENTMGR_H