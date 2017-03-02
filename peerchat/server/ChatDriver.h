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

#include <OS/User.h>
#include <OS/Profile.h>

#define CHAT_PING_TIME 30

namespace Chat {
	class Peer;

	enum EOperPrivileges {
		OPERPRIVS_NONE = 0,
		OPERPRIVS_INVISIBLE = 1<<0,
		OPERPRIVS_BANEXCEMPT = 1<<1,
		OPERPRIVS_GETOPS = 1<<2,
		OPERPRIVS_GLOBALOWNER = 1<<3,
		OPERPRIVS_GETVOICE = 1<<4,
		OPERPRIVS_OPEROVERRIDE = 1<<5,
		OPERPRIVS_WALLOPS = 1<<6,
		OPERPRIVS_KILL = 1<<7,
		OPERPRIVS_FLOODEXCEMPT = 1<<8,
		OPERPRIVS_LISTOPERS = 1<<9,
		OPERPRIVS_CTCP = 1<<10, //and ATM, etc
		OPERPRIVS_HIDDEN = 1 << 11,
		OPERPRIVS_SEEHIDDEN = 1 << 12,
		OPERPRIVS_MANIPULATE = 1 << 13, //can manipulate other peoples keys, etc
		OPERPRIVS_SERVMANAGE = 1 << 14,
		OPERPRIVS_WEBPANEL = 1 << 15, //permitted to log in to the web panel(doesn't serve any use on the actual server)
	};
	enum EUserMode {
		EUserMode_None = 0,
		EUserMode_Quiet = 1 << 0,
		EUserMode_ShowConns = 1 << 1, //oper mode(OPERPRIVS_LISTOPERS)
		EUserMode_ShowJoins = 1 << 2, //oper mode(OPERPRIVS_LISTOPERS)
		EUserMode_SpyMessages = 1 << 3, //oper mode(OPERPRIVS_LISTOPERS)
		EUserMode_HideSpyMessages = 1 << 4, //oper mode(OPERPRIVS_MANIPULATE)
		EUserMode_AllowInvisiblePrivmsg = 1 << 5, //oper mode, allows invisible users to send channel privmsgs to non-opers(OPERPRIVS_INVISIBLE)
	};

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

		bool HasPeer(Chat::Peer *peer);


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

		OS::User m_user;
		OS::Profile m_profile;
		int m_operflags;

	};
}
#endif //_SBDRIVER_H