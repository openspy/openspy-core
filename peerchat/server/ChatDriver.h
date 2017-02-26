#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "ChatPeer.h"
#include "ChatBackend.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define CHAT_PING_TIME 30

namespace Chat {
	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void tick(fd_set *fdset);
		void think(fd_set *fdset);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		Peer *find_or_create(struct sockaddr_in *address);

		int setup_fdset(fd_set *fdset);


		int GetNumConnections();

		bool HasPeer(Chat::Peer * peer);


		void OnSendClientMessage(int target_id, ChatClientInfo from_user, const char *msg, EChatMessageType message_type);
		void OnSendChannelMessage(ChatChannelInfo channel, ChatClientInfo from_user, const char *msg, EChatMessageType message_type);
		void SendJoinChannelMessage(ChatClientInfo client, ChatChannelInfo channel);
		void SendPartChannelMessage(ChatClientInfo client, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str);
		void SendChannelModeUpdate(ChatClientInfo client_info, ChatChannelInfo channel_info, ChanModeChangeData change_data);
		void SendUpdateChannelTopic(ChatClientInfo client, ChatChannelInfo channel);
		void SendSetChannelClientKeys(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data);
		void SendSetChannelKeys(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data);
		void SendUserQuitMessage(ChatClientInfo client, std::string quit_reason);
	private:

		void TickConnections(fd_set *fdset);

		int m_sd;

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

	};
}
#endif //_SBDRIVER_H