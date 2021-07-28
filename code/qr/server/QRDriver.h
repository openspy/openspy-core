#ifndef _QRDRIVER_H
#define _QRDRIVER_H
#include <stdint.h>
#include <queue>
#include <OS/OpenSpy.h>
#include <OS/Mutex.h>
#include <OS/Net/NetDriver.h>


#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define MAX_DATA_SIZE 1400
#define DRIVER_THREAD_TIME 1000
namespace MM {
	class MMTaskResponse;
}
namespace QR {
	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool listener_waiting);

		//v2
		void handle_v2_packet(INetIODatagram &packet);		
		void handle_v2_available(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_heartbeat(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_challenge(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_keepalive(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);
		void handle_v2_client_message_ack(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);
		

		void send_client_message(int version, OS::Address to_address, uint32_t instance_key, uint32_t message_key, OS::Buffer &send_buffer);

		void send_v2_error(OS::Address to, uint32_t instance_key, uint8_t error_code, const char *error_message);


		static void on_keepalive_processed(MM::MMTaskResponse response);
		static void on_got_v2_available_data(MM::MMTaskResponse response);
		static void on_heartbeat_processed(MM::MMTaskResponse response);
		static void on_got_v2_challenge_response(MM::MMTaskResponse response);		

		void SendPacket(OS::Address to, OS::Buffer buffer);

		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
		void OnPeerMessage(INetPeer *peer);
	private:
		struct timeval m_server_start;

		INetIOSocket *mp_socket;

	};
}
#endif //_QRDRIVER_H