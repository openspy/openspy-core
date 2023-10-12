#ifndef _V2PEER_H
#define _V2PEER_H
#include "SBPeer.h"
#include "SBDriver.h"
#include "sb_crypt.h"
#include <map>
#include <string>

#include <OS/Mutex.h>
#include <OS/Buffer.h>

//message types for outgoing requests
#define SERVER_LIST_REQUEST		0
#define SERVER_INFO_REQUEST		1
#define SEND_MESSAGE_REQUEST	2
#define KEEPALIVE_REPLY			3
#define MAPLOOP_REQUEST			4
#define PLAYERSEARCH_REQUEST	5

//message types for incoming requests
#define PUSH_KEYS_MESSAGE		1
#define PUSH_SERVER_MESSAGE		2
#define KEEPALIVE_MESSAGE		3
#define DELETE_SERVER_MESSAGE	4
#define MAPLOOP_MESSAGE			5
#define PLAYERSEARCH_MESSAGE	6

//server list update options
#define SEND_FIELDS_FOR_ALL		1
#define NO_SERVER_LIST			2
#define PUSH_UPDATES			4
#define SEND_GROUPS				32
#define NO_LIST_CACHE			64
#define LIMIT_RESULT_COUNT		128

#define ALTERNATE_SOURCE_IP 8


#define CRYPTCHAL_LEN 10
#define SERVCHAL_LEN 25
#define LIST_CHALLENGE_LEN 8


//game server flags
#define UNSOLICITED_UDP_FLAG	1
#define PRIVATE_IP_FLAG			2
#define CONNECT_NEGOTIATE_FLAG	4
#define ICMP_IP_FLAG			8
#define NONSTANDARD_PORT_FLAG	16
#define NONSTANDARD_PRIVATE_PORT_FLAG	32
#define HAS_KEYS_FLAG					64
#define HAS_FULL_RULES_FLAG				128

namespace SB {
		class V2Peer : public Peer {
			public:
				V2Peer(Driver *driver, uv_tcp_t *sd);
				~V2Peer();
				void think(bool packet_waiting);
				void informDeleteServers(MM::Server *server);
				void informNewServers(MM::Server *server);
				void informUpdateServers(MM::Server *server);
				void OnRecievedGameInfo(const OS::GameData game_data, void *extra);
				void OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra);
			private:

				void on_stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
				void SendPacket(uint8_t *buff, size_t len, bool prepend_length);
				void send_ping();
				void handle_packet(OS::Buffer &buffer);
				size_t setupCryptHeader(OS::Buffer &buffer);

				//request type handles
				void	ProcessListRequest(OS::Buffer &buffer);
				void	ProcessSendMessage(OS::Buffer &buffer);
				void	ProcessInfoRequest(OS::Buffer &buffer);

				//request processors
				MM::sServerListReq ParseListRequest(OS::Buffer &buffer);

				void OnRetrievedServers(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);
				void OnRetrievedGroups(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);
				void OnRetrievedServerInfo(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);

				void SendListQueryResp(MM::ServerListQuery servers, const MM::sServerListReq list_req, bool usepopularlist = true, bool send_fullkeys = false);
				
				void sendServerData(MM::Server *server, bool usepopularlist, bool push, OS::Buffer *sendBuffer, bool full_keys = false, const std::map<std::string, int> *optimized_fields = NULL, bool no_keys = false, bool first_set = false);
				void WriteOptimizedField(MM::ServerListQuery servers, std::string field_name, OS::Buffer &buffer, std::map<std::string, int> &field_types);
				void SendPushKeys();
				void send_error(bool die, const char *fmt, ...);

				bool m_sent_crypt_header;
				uint8_t m_challenge[LIST_CHALLENGE_LEN];

				bool m_next_packet_send_msg;
				struct sockaddr_in m_send_msg_to;

				GOACryptState m_crypt_state;

				bool m_sent_push_keys;
				bool m_got_game_pair;
				bool m_in_message;

				OS::Buffer *mp_push_delay_buffer;

				#if HACKER_PATCH_MSG_SPAM_CHECKER
					int m_hp_msg_spam_count;
					OS::Address m_hp_msg_spam_last_msg_sent_to;
				#endif
		};

}
#endif //_V2PEER_H
