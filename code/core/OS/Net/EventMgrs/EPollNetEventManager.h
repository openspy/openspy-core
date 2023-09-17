#ifndef _EPOLLNETEVENTMGR_H
#define _EPOLLNETEVENTMGR_H
#include <OS/OpenSpy.h>
#include <sys/epoll.h>
	#if EVTMGR_USE_EPOLL
		#include <OS/Net/NetEventManager.h>
		#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
		#include <OS/LinkedList.h>
		#define MAX_EPOLL_EVENTS 4096
		#define EPOLL_TIMEOUT 15


		class EPollNetEventManager;

		class EPollNetEventManager : public INetEventManager, public BSDNetIOInterface<> {
		public:
			EPollNetEventManager();
			~EPollNetEventManager();

			void RegisterSocket(INetPeer *peer, bool notify_driver_only = false);
			void UnregisterSocket(INetPeer *peer);
			void UnregisterDriver(INetDriver *driver);
			void run();
		private:
			int m_epoll_drivers_fd;
			int m_epoll_peers_fd;
			void setupDrivers();
			
			bool m_added_drivers;
		};
	#endif
#endif //_EPOLLNETEVENTMGR_H
