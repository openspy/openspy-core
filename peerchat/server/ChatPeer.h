#ifndef _CHATPEER_H
#define _CHATPEER_H
#include "../main.h"
#include "ChatDriver.h"
#include "ChatBackend.h"
#include <vector>

#define CHAT_MAX_MESSAGE 600

namespace Chat {
	class Driver;
	class Server;

	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		virtual ~Peer();
		
		virtual void think(bool packet_waiting) = 0;
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };



		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		int GetPing();

		ChatClientInfo getClientInfo() { return m_client_info; };

		//p->OnRecvClientMessage(from_user, msg);
		virtual void OnRecvClientMessage(ChatClientInfo from_user, const char *msg) = 0;
		virtual void OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel) = 0;
		virtual void OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel) = 0;
		virtual void OnRecvChannelModeUpdate(ChatClientInfo user, ChatChannelInfo channel, ChanModeChangeData change_data) = 0;
		virtual void OnChannelTopicUpdate(ChatClientInfo user, ChatChannelInfo channel) = 0;
		bool IsOnChannel(ChatChannelInfo channel);

	protected:

		std::vector<int> m_channel_list;

		int m_sd;
		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		ChatClientInfo m_client_info;

		OS::CMutex *mp_mutex;
	private:




	};
}
#endif //_SAMPRAKPEER_H