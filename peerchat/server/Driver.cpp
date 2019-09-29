#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <OS/SharedTasks/tasks.h>
#include "Driver.h"
namespace Peerchat {
    Driver::Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {

    }
    Peer *Driver::FindPeerByProfileID(int profileid) {
        return NULL;
    }
    Peer *Driver::FindPeerByUserSummary(std::string summary_string) {
		mp_mutex->lock();
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = (Peer *)*it;
			if (stricmp(peer->GetUserDetails().ToString().c_str(), summary_string.c_str()) == 0 && !peer->ShouldDelete()) {
				mp_mutex->unlock();
				return peer;
			}
			it++;
		}
		mp_mutex->unlock();
		return NULL;   
    }
    INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
        return new Peer(this, socket);
    }
	void Driver::SendUserMessageToVisibleUsers(std::string fromSummary, std::string messageType, std::string message, bool includeSelf = true) {
		mp_mutex->lock();
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = (Peer *)*it;
			bool summaryMatch = peer->GetUserDetails().ToString().compare(fromSummary);
			if(summaryMatch && includeSelf) {
				peer->send_message(messageType, message, fromSummary);
			} else if(!summaryMatch) {
				peer->send_message(messageType, message, fromSummary);
			}
			it++;
		}
		mp_mutex->unlock();
	}
	void Driver::OnChannelMessage(std::string type, std::string from, std::string to, std::string message) {
		mp_mutex->lock();
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = (Peer *)*it;
			peer->send_message(type, message,from, to);
			it++;
		}
		mp_mutex->unlock();
	}
}