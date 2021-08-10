#ifndef _CDKEYDRIVER_H
#define _CDKEYDRIVER_H
#include <stdint.h>
#include <OS/Net/NetDriver.h>

#include <queue>
#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <OS/KVReader.h>

#define MAX_DATA_SIZE 1400
#define DRIVER_THREAD_TIME 1000
namespace CDKey {
	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool packet_waiting);

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
		void OnPeerMessage(INetPeer *peer);

		void handle_auth_packet(OS::Address from, OS::KVReader data_parser);
		void SendPacket(OS::Address to, std::string message);
	private:
		static void *TaskThread(OS::CThread *thread);

		struct timeval m_server_start;

		OS::CMutex *mp_mutex;

		INetIOSocket *mp_socket;
	};
}
#endif //_NNDRIVER_H