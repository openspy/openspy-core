#include "ChatPeer.h"
#include "ChatDriver.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

#include <algorithm>

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

		m_operflags = 0;
		m_user.id = 0;
		m_profile.id = 0;

		mp_mutex = OS::CreateMutex();

		OS::LogText(OS::ELogLevel_Info, "New connection from %s",OS::Address(m_address_info).ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "Connection from %s closed",OS::Address(m_address_info).ToString().c_str());
		close(m_sd);
		delete mp_mutex;
	}
	bool Peer::IsOnChannel(ChatChannelInfo channel) {
		return std::find(m_channel_list.begin(),m_channel_list.end(), channel.channel_id) != m_channel_list.end();
	}
}