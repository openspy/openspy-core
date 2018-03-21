#ifndef _SELECTNETEVENTMGR_H
	#if EVTMGR_USE_SELECT
		#define _SELECTNETEVENTMGR_H
		#include <OS/OpenSpy.h>
		#include "NetEventManager.h"
		#include "NetIOInterface.h"
		#include <vector>

		#define SELECT_TIMEOUT 5

		class SelectNetEventManager : public INetEventManager, public INetIOInterface {
		public:
			SelectNetEventManager();
			~SelectNetEventManager();

			void RegisterSocket(INetPeer *peer);
			void UnregisterSocket(INetPeer *peer);
			void run();

			//NET IO INTERFACE
			INetIOSocket *BindTCP(OS::Address bind_address);
			std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket);
			NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer);
			NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer);

			INetIOSocket *BindUDP(OS::Address bind_address);
			NetIOCommResp datagramRecv(INetIOSocket *socket, std::vector<INetIODatagram> &datagrams);
			NetIOCommResp datagramSend(INetIOSocket *socket, OS::Buffer &buffer);
			void closeSocket(INetIOSocket *socket);

			void makeNonBlocking(int sd);
			//
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