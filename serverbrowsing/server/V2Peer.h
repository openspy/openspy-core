#ifndef _V2PEER_H
#define _V2PEER_H
#include "SBPeer.h"
#include "SBDriver.h"
#include "sb_crypt.h"
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
				V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
				~V2Peer();
				void think(bool packet_waiting);
				void informDeleteServers(MM::ServerListQuery servers);
				void informNewServers(MM::ServerListQuery servers);
				void informUpdateServers(MM::ServerListQuery servers);
			private:



				
				void SendPacket(uint8_t *buff, int len, bool prepend_length);
				void send_ping();
				void handle_packet(char *data, int len);
				void setupCryptHeader(uint8_t **dst, int *len);

				//request type handles
				void ProcessListRequset(uint8_t *buffer, int remain);
				void ProcessSendMessage(uint8_t *buffer, int remain);
				void ProcessInfoRequest(uint8_t *buffer, int remain);

				//request processors
				sServerListReq ParseListRequest(uint8_t *buffer, int remain);

				void SendListQueryResp(struct MM::ServerListQuery servers, sServerListReq *list_req, bool usepopularlist = true, bool send_fullkeys = false);
				void sendServerData(MM::Server *server, bool usepopularlist, bool push, uint8_t **out, int *out_len, bool full_keys = false);
				void SendPushKeys();
				void send_error(bool die, const char *fmt, ...);

				bool m_sent_crypt_header;
				uint8_t m_challenge[LIST_CHALLENGE_LEN];
				unsigned char   encxkeyb[261];


				bool m_next_packet_send_msg;
				struct sockaddr_in m_send_msg_to;

				GOACryptState m_crypt_state;
		};

}
#endif //_V2PEER_H