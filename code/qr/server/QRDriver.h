#ifndef _QRDRIVER_H
#define _QRDRIVER_H
#include <stdint.h>
#include <queue>
#include <OS/OpenSpy.h>
#include <OS/Mutex.h>
#include <OS/Net/NetDriver.h>

#include <uv.h>


#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <OS/Net/Processors/KVProcessor.h>

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

		static void on_udp_read(uv_udp_t* handle,
                               ssize_t nread,
                               const uv_buf_t* buf,
                               const struct sockaddr* addr,
                               unsigned flags);

		static void on_send_callback(uv_udp_send_t* req, int status);

		//v1
		void perform_v1_key_scan(OS::Address from_address);
		void handle_v1_packet(OS::Address address, OS::Buffer buffer);
		void handle_v1_heartbeat(OS::Address from_address, OS::KVReader reader);
		void handle_v1_heartbeat_data(OS::Address from_address, OS::KVReader reader);
		void handle_v1_validate(OS::Address from_address, OS::KVReader reader);
		void handle_v1_echo(OS::Address from_address, OS::KVReader reader);

		static void on_v1_heartbeat_data_processed(MM::MMTaskResponse response);
		static void on_v1_heartbeat_processed(MM::MMTaskResponse response);
		static void on_v1_secure_processed(MM::MMTaskResponse response);
		static void on_v1_echo_processed(MM::MMTaskResponse response);
		

		//v2
		void handle_v2_packet(OS::Address address, OS::Buffer buffer);		
		void handle_v2_available(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_heartbeat(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_challenge(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);		
		void handle_v2_keepalive(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);
		void handle_v2_client_message_ack(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer);
		

		void send_client_message(int version, OS::Address to_address, uint32_t instance_key, uint32_t message_key, OS::Buffer &send_buffer);

		void send_v2_error(OS::Address to, uint32_t instance_key, uint8_t error_code, const char *error_message);
		void send_v1_error(OS::Address to, const char *error_message);


		static void on_v2_keepalive_processed(MM::MMTaskResponse response);
		static void on_got_v2_available_data(MM::MMTaskResponse response);
		static void on_v2_heartbeat_processed(MM::MMTaskResponse response);
		static void on_got_v2_challenge_response(MM::MMTaskResponse response);		

		INetIOSocket *getListenerSocket() const;
		struct sockaddr_in GetAddress() { return m_recv_addr; };
		const std::vector<INetIOSocket *> getSockets() const;
		void OnPeerMessage(INetPeer *peer);

		void handle_v1_packet(OS::KVReader data_parser);

		void AddRequest(MM::MMPushRequest req);
	private:
		struct timeval m_server_start;

	};
}
#endif //_QRDRIVER_H