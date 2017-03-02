#ifndef _CHATPEER_H
#define _CHATPEER_H
#include "../main.h"
#include "ChatDriver.h"
#include "ChatBackend.h"
#include <vector>

#include <OS/Auth.h>
#include <OS/Search/Profile.h>
#include <OS/Search/User.h>

#define CHAT_MAX_MESSAGE 600

namespace Chat {
	class Driver;
	class Server;
	class Peer;

	typedef struct {
		int m_op_hits;
		int m_hits;
	} ChannelHitCounts;

	typedef struct {
		Chat::Driver *driver;
		Chat::Peer *peer;
		int extra;
	} ChatCallbackContext;

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
		virtual void OnRecvChannelMessage(ChatClientInfo from_user, ChatChannelInfo to_channel, const char *msg, EChatMessageType message_type) = 0;
		virtual void OnRecvClientMessage(ChatClientInfo from_user, const char *msg, EChatMessageType message_type) = 0;
		virtual void OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel) = 0;
		virtual void OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str) = 0;
		virtual void OnRecvChannelModeUpdate(ChatClientInfo user, ChatChannelInfo channel, ChanModeChangeData change_data) = 0;
		virtual void OnChannelTopicUpdate(ChatClientInfo user, ChatChannelInfo channel) = 0;
		virtual void OnSendSetChannelClientKeys(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data) = 0;
		virtual void OnSendSetChannelKeys(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data) = 0;
		virtual void OnUserQuit(ChatClientInfo client, std::string quit_reason) = 0;
		bool IsOnChannel(ChatChannelInfo channel);
		void GetChannelList(std::vector<int> &list) { list = m_channel_list; };
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

		std::map<int, ChannelHitCounts> m_client_channel_hits; //number of instances a user sees a client(how many channels they share) - [client_id].m_hits++, m_op_hits = num hits on channels you are op of

		OS::Profile m_profile;
		OS::User m_user;
		int m_operflags;
	private:




	};
}
#endif //_SAMPRAKPEER_H