#ifndef _SELECTNETEVENTMGR_H
	#if EVTMGR_USE_SELECT
		#define _SELECTNETEVENTMGR_H
		#include <OS/OpenSpy.h>
		#include <OS/Net/NetEventManager.h>
		#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
		#include <vector>

		#define SELECT_TIMEOUT 5

		class SelectNetEventManager : public INetEventManager, public BSDNetIOInterface<> {
		public:
			SelectNetEventManager();
			~SelectNetEventManager();

			void RegisterSocket(INetPeer *peer, bool notify_driver_only = false);
			void UnregisterSocket(INetPeer *peer);
			void run();

		private:
			socktype_t setup_fdset();
			fd_set  m_fdset;

			socktype_t m_hsock;
			OS::CMutex *mp_mutex;
		};
	#endif
#endif //_SELECTNETEVENTMGR_H