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
		void flagExit();
	protected:
		bool m_exit_flag;
		std::vector<INetDriver *> m_net_drivers;
};
#endif //_NETEVENTMGR_H