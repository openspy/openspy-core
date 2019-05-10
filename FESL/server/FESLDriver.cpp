#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"


namespace FESL {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, PublicInfo public_info, std::string str_crypter_rsa_key, const char *x509_path, const char *rsa_priv_path, SSLNetIOIFace::ESSL_Type ssl_version, bool proxyFlag = false) : TCPDriver(server, host, port, proxyFlag, x509_path, rsa_priv_path, ssl_version) {

		//setup config vars
		m_server_info.domainPartition = public_info.domainPartition;
		m_server_info.subDomain = public_info.subDomain;
		m_server_info.messagingHostname = public_info.messagingHostname;
		m_server_info.messagingPort = public_info.messagingPort;
		m_server_info.theaterHostname = public_info.theaterHostname;
		m_server_info.theaterPort = public_info.theaterPort;
		m_server_info.termsOfServiceData = public_info.termsOfServiceData;

		//mp_socket_interface = new SSLNetIOIFace::SSLNetIOInterface(ssl_version, rsa_priv_path, x509_path);		

		//mp_socket = mp_socket_interface->BindTCP(address);

		mp_string_crypter = new OS::StringCrypter(str_crypter_rsa_key);
	}
	Driver::~Driver() {
		delete mp_string_crypter;
	}
	void Driver::OnUserAuth(std::string session_key, int userid, int profileid) {
		mp_mutex->lock();
		std::deque<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = (Peer*)*it;
			if(peer == NULL) break;
			OS::User user;
			OS::Profile profile;
			if(!peer->ShouldDelete() && peer->GetAuthCredentials(user, profile)) {
				if(user.id == userid && profile.id == profileid && peer->getSessionKey().compare(session_key) == 0) {
					peer->DuplicateLoginExit();
				}
			}
			it++;
		}
		mp_mutex->unlock();
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		return new Peer(this, socket);
	}
}