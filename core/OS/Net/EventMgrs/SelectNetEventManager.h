#ifndef _SELECTNETEVENTMGR_H
	#if EVTMGR_USE_SELECT
		#define _SELECTNETEVENTMGR_H
		#include <OS/OpenSpy.h>
		#include <OS/Net/NetEventManager.h>
		#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
		#include <vector>

		#define SELECT_TIMEOUT 5

		class SelectNetEventManager : public INetEventManager, public BSDNetIOInterface {
		public:
			SelectNetEventManager();
			~SelectNetEventManager();

			void RegisterSocket(INetPeer *peer);
			void UnregisterSocket(INetPeer *peer);
			void run();

		private:
			int setup_fdset();
			fd_set  m_fdset;

			bool m_dirty_fdset;
			std::vector<int> m_cached_sockets;
			int m_hsock;

			std::vector<INetPeer *> m_peers;
			OS::CMutex *mp_mutex;
		};
	#endif
#endif //_SELECTNETEVENTMGR_H