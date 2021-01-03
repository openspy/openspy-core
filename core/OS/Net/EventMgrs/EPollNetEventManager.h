#ifndef _EPOLLNETEVENTMGR_H
#define _EPOLLNETEVENTMGR_H
#include <OS/OpenSpy.h>
#include <sys/epoll.h>
	#if EVTMGR_USE_EPOLL
		#include <OS/Net/NetEventManager.h>
		#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
		#include <OS/LinkedList.h>
		#define MAX_EPOLL_EVENTS 4096
		#define EPOLL_TIMEOUT 200

		class EPollDataInfo : public OS::LinkedList<EPollDataInfo *>  {
			public:
				EPollDataInfo() : OS::LinkedList<EPollDataInfo *>() {
					is_peer = false;
					ptr = NULL;
					is_peer_notify_driver = false;
				}
				bool is_peer;
				bool is_peer_notify_driver;
				void *ptr; //peer or driver (driver for UDP, peer for TCP)
		};

		class EPollNetEventManager;
		typedef struct {
			EPollNetEventManager *event_manager;
			INetPeer *unregisterTarget;
		} UnregisterSocketIteratorState;

		class EPollNetEventManager : public INetEventManager, public BSDNetIOInterface<> {
		public:
			EPollNetEventManager();
			~EPollNetEventManager();

			void RegisterSocket(INetPeer *peer, bool notify_driver_only = false);
			void UnregisterSocket(INetPeer *peer);
			void run();
		private:
			static bool LLIterator_UnregisterSocket(EPollDataInfo* data_info, UnregisterSocketIteratorState* peer);
			int m_epollfd;
			struct epoll_event m_events[MAX_EPOLL_EVENTS];
			
			void setupDrivers();
			
			bool m_added_drivers;

			OS::LinkedListHead<EPollDataInfo *> *mp_data_info_head;
		};
	#endif
#endif //_EPOLLNETEVENTMGR_H
