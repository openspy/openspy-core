#include "SBPeer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

namespace SB {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
	}
	Peer::~Peer() {

	}
	bool Peer::serverMatchesLastReq(MM::Server *server, bool require_push_flag) {
		/*
		if(require_push_flag && !m_last_list_req.push_updates) {
			return false;
		}*/
		if(server->game.gameid == m_last_list_req.m_for_game.gameid) {
			return true;
		}
		return false;
	}
}