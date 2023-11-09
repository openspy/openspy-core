#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"


namespace FESL {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, PublicInfo public_info, std::string str_crypter_rsa_key, void *ssl_ctx) : SSLTCPDriver(server, host, port, ssl_ctx) {

		//setup config vars
		m_server_info.domainPartition = public_info.domainPartition;
		m_server_info.subDomain = public_info.subDomain;
		m_server_info.messagingHostname = public_info.messagingHostname;
		m_server_info.messagingPort = public_info.messagingPort;
		m_server_info.theaterHostname = public_info.theaterHostname;
		m_server_info.theaterPort = public_info.theaterPort;
		m_server_info.termsOfServiceData = public_info.termsOfServiceData;
		m_server_info.gameid = public_info.gameid;

		mp_string_crypter = new OS::StringCrypter(str_crypter_rsa_key);
	}
	Driver::~Driver() {
		delete mp_string_crypter;
	}
	void Driver::OnUserAuth(std::string session_key, int userid, int profileid) {
		//mp_mutex->lock();
		Peer* peer = (Peer*)GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				OS::User user;
				OS::Profile profile;
				if (peer->GetAuthCredentials(user, profile)) {
					if (user.id == userid && profile.id == profileid && peer->getSessionKey().compare(session_key) == 0) {
						peer->DuplicateLoginExit();
					}
				}
			} while ((peer = (Peer*)peer->GetNext()) != NULL);
		}
		//mp_mutex->unlock();
	}
	INetPeer *Driver::CreatePeer(uv_tcp_t *sd) {
		return new Peer(this, sd);
	}
}