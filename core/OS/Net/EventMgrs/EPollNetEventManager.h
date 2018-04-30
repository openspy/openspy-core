#ifndef _EPOLLNETEVENTMGR_H
#define _EPOLLNETEVENTMGR_H
#include <OS/OpenSpy.h>
#include <sys/epoll.h>
	#if EVTMGR_USE_EPOLL
		#include <OS/Net/NetEventManager.h>
		#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
		#include <vector>
		#include <map>
		#define MAX_EPOLL_EVENTS 4096
		#define EPOLL_TIMEOUT 200

		typedef struct {
			bool is_peer;
			void *ptr;
		} EPollDataInfo;
		class EPollNetEventManager : public INetEventManager, public BSDNetIOInterface<> {
		public:
			EPollNetEventManager();
			~EPollNetEventManager();

			void RegisterSocket(INetPeer *peer);
			void UnregisterSocket(INetPeer *peer);
			void run();
		private:
			int m_epollfd;
			struct epoll_event m_events[MAX_EPOLL_EVENTS];
			
			void setupDrivers();
			
			bool m_added_drivers;
			
			std::map<void *, EPollDataInfo *> m_datainfo_map;
		};
	#endif
#endif //_EPOLLNETEVENTMGR_H
