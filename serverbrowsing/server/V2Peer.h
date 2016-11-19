#ifndef _V2PEER_H
#define _V2PEER_H
#include "SBPeer.h"
#include "SBDriver.h"
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

				bool m_sent_crypt_header;
				uint8_t m_challenge[LIST_CHALLENGE_LEN];
				unsigned char   encxkeyb[261];


				bool m_next_packet_send_msg;
				struct sockaddr_in m_send_msg_to;
		};

}
#endif //_V2PEER_H