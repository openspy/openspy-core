#ifndef _NNDRIVER_H
#define _NNDRIVER_H
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

#define MAX_DATA_SIZE 1400
#define DRIVER_THREAD_TIME 1000
namespace NN {
	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool packet_waiting);

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
		void OnPeerMessage(INetPeer *peer);
		void SendPacket(OS::Address to, NatNegPacket *packet);
	private:
		static void *TaskThread(OS::CThread *thread);

		int packetSizeFromType(uint8_t type, uint8_t version);
		void handle_init_packet(OS::Address from, NatNegPacket *packet, std::string gamename);
		void handle_connect_ack_packet(OS::Address from, NatNegPacket *packet);
		void handle_address_check_packet(OS::Address from, NatNegPacket *packet);
		void handle_report_packet(OS::Address from, NatNegPacket *packet);
		void handle_preinit_packet(OS::Address from, NatNegPacket *packet);
		void handle_natify_packet(OS::Address from, NatNegPacket *packet);
		void handle_ert_ack_packet(OS::Address from, NatNegPacket *packet);


		struct timeval m_server_start;

		OS::CMutex *mp_mutex;

		INetIOSocket *mp_socket;
	};
}
#endif //_NNDRIVER_H