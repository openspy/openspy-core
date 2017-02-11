#include "ChatPeer.h"
#include "ChatDriver.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

#include "ChatBackend.h"
namespace Chat {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);

		m_client_info.client_id = 0;
		m_client_info.name = "";
		m_client_info.user = "";
		m_client_info.realname = "";
		m_client_info.hostname = "*";

		mp_mutex = OS::CreateMutex();
	}
	Peer::~Peer() {
		close(m_sd);
		delete mp_mutex;
	}

	void Peer::handle_queues() {
		mp_mutex->lock();
		std::vector<MessageSendQueueData>::iterator it = m_client_msg_send_queue.begin();
		while(it != m_client_msg_send_queue.end()) {
			MessageSendQueueData data = *it;
			ChatBackendTask::SubmitClientMessage(data.client_id, data.message, NULL, this, NULL);
			it++;
		}

		m_client_msg_send_queue.clear();
		mp_mutex->unlock();
	}
}