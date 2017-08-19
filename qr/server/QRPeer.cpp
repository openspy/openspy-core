#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/helpers.h>


namespace QR {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_server_pushed = false;
		m_delete_flag = false;
		m_timeout_flag = false;
		m_sd = sd;
		m_address_info = *address_info;
		m_server_info.id = -1;
		m_server_info.m_game.gameid = 0;
		m_server_info.m_game.gamename[0] = 0;
		m_sent_game_query = false;
		OS::LogText(OS::ELogLevel_Info, "New connection from %s",OS::Address(m_address_info).ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "Connection from %s closed",OS::Address(m_address_info).ToString().c_str());
	}
	bool Peer::isTeamString(const char *string) {
		int len = strlen(string);
		if(len < 2)
			return false;
		if(string[len-2] == '_' && string[len-1] == 't') {
			return true;
		}
		return false;
	}

}