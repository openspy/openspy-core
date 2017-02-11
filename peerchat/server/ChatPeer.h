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

	typedef struct {
		int client_id;
		std::string message;
	} MessageSendQueueData;


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

		void handle_queues();

		//p->OnRecvClientMessage(from_user, msg);
		virtual void OnRecvClientMessage(ChatClientInfo from_user, const char *msg) = 0;

	protected:


		int m_sd;
		Driver *mp_driver;

		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		ChatClientInfo m_client_info;

		//queue variables
		std::vector<MessageSendQueueData> m_client_msg_send_queue;
		//

		OS::CMutex *mp_mutex;
	private:




	};
}
#endif //_SAMPRAKPEER_H