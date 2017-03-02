#include "ChatDriver.h"
#include <stdio.h>
#include <stdlib.h>

#include "ChatServer.h"
#include <OS/legacy/buffwriter.h>

#include "ChatPeer.h"
#include "IRCPeer.h"

#include <OS/socketlib/socketlib.h>

namespace Chat {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (Socket::setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		m_local_addr.sin_port = Socket::htons(port);
		m_local_addr.sin_addr.s_addr = Socket::htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = Socket::bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}
		if (Socket::listen(m_sd, SOMAXCONN)
			< 0) {
			//signal error
		}

		gettimeofday(&m_server_start, NULL);
		Chat::ChatBackendTask::getQueryTask()->AddDriver(this);
	}
	Driver::~Driver() {
		Chat::ChatBackendTask::getQueryTask()->RemoveDriver(this);
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			delete peer;
			it++;
		}
	}
	void Driver::think(fd_set *fdset) {

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			if (!peer->ShouldDelete())
				peer->think(FD_ISSET(peer->GetSocket(), fdset));
			else {
				//delete if marked for deletiontel
				it = m_connections.erase(it);
				delete peer;
				
				continue;
			}
			it++;
		}
	}

	Peer *Driver::find_client(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		return NULL;
	}
	Peer *Driver::find_or_create(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		Peer *ret = new IRCPeer(this, address, m_sd);
		m_connections.push_back(ret);
		return ret;
	}
	void Driver::tick(fd_set *fdset) {

		TickConnections(fdset);
		if (!FD_ISSET(m_sd, fdset)) {
			return;
		}
		socklen_t psz = sizeof(struct sockaddr_in);
		struct sockaddr_in peer;
		int sda = Socket::accept(m_sd, (struct sockaddr *)&peer, &psz);
		if (sda <= 0) return;
		IRCPeer *mp_peer = new IRCPeer(this, &peer, sda);

		m_connections.push_back(mp_peer);
		mp_peer->think(true);

	}

	bool Driver::HasPeer(Chat::Peer * peer) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			if (*it == peer) {
				return true;
			}
			it++;
		}
		return false;
	}


	int Driver::getListenerSocket() {
		return m_sd;
	}

	uint32_t Driver::getBindIP() {
		return htonl(m_local_addr.sin_addr.s_addr);
	}
	uint32_t Driver::getDeltaTime() {
		struct timeval now;
		gettimeofday(&now, NULL);
		uint32_t t = (now.tv_usec / 1000.0) - (m_server_start.tv_usec / 1000.0);
		return t;
	}


	int Driver::GetNumConnections() {
		return m_connections.size();
	}


	int Driver::setup_fdset(fd_set *fdset) {

		int hsock = m_sd;
		FD_SET(m_sd, fdset);
		
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			int sd = p->GetSocket();
			FD_SET(sd, fdset);
			if (sd > hsock)
				hsock = sd;
			it++;
		}
		return hsock + 1;
	}

	void Driver::TickConnections(fd_set *fdset) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(FD_ISSET(p->GetSocket(), fdset));
			it++;
		}
	}
	void Driver::OnSendClientMessage(int target_id, ChatClientInfo from_user, const char *msg, EChatMessageType message_type) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(info.client_id == target_id) {
				p->OnRecvClientMessage(from_user, msg, message_type);
			}
			it++;
		}
	}
	void Driver::OnSendChannelMessage(ChatChannelInfo channel, ChatClientInfo from_user, const char *msg, EChatMessageType message_type) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(p->IsOnChannel(channel)) {
				p->OnRecvChannelMessage(from_user, channel, msg, message_type);
			}
			it++;
		}
	}
	void Driver::SendJoinChannelMessage(ChatClientInfo client, ChatChannelInfo channel) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(info.client_id == client.client_id || p->IsOnChannel(channel)) {
				p->OnRecvClientJoinChannel(client, channel);
			}
			it++;
		}
	}
	void Driver::SendPartChannelMessage(ChatClientInfo client, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(info.client_id == client.client_id || p->IsOnChannel(channel)) {
				p->OnRecvClientPartChannel(client, channel, part_reason, reason_str);
			}
			it++;
		}
	}
	void Driver::SendChannelModeUpdate(ChatClientInfo client_info, ChatChannelInfo channel_info, ChanModeChangeData change_data) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(p->IsOnChannel(channel_info)) {
				p->OnRecvChannelModeUpdate(client_info, channel_info,change_data);
			}
			it++;
		}
	}
	void Driver::SendUpdateChannelTopic(ChatClientInfo client, ChatChannelInfo channel) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(p->IsOnChannel(channel)) {
				p->OnChannelTopicUpdate(client, channel);
			}
			it++;
		}
	}
	void Driver::SendSetChannelClientKeys(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(p->IsOnChannel(channel)) {
				p->OnSendSetChannelClientKeys(client, channel, kv_data);
			}
			it++;
		}
	}
	void Driver::SendSetChannelKeys(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			if(p->IsOnChannel(channel)) {
				p->OnSendSetChannelKeys(client, channel, kv_data);
			}
			it++;
		}
	}
	void Driver::SendUserQuitMessage(ChatClientInfo client, std::string quit_reason) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			ChatClientInfo info = p->getClientInfo();
			p->OnUserQuit(client, quit_reason);
			it++;
		}
	}
}