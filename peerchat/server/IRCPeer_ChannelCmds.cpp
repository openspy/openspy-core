#include <OS/OpenSpy.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetServer.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/gsmsalg.h>
#include <OS/legacy/enctype_shared.h>
#include <OS/legacy/helpers.h>


#include <sstream>
#include "ChatDriver.h"
#include "ChatServer.h"
#include "ChatPeer.h"
#include "IRCPeer.h"
#include "ChatBackend.h"


#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>

#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>


namespace Chat {
		EIRCCommandHandlerRet IRCPeer::handle_join(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			printf("Join channel: %s\n", channel.c_str());
			ChatBackendTask::SubmitFind_OrCreateChannel(OnJoinCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
		EIRCCommandHandlerRet IRCPeer::handle_part(std::vector<std::string> params, std::string full_params) {
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnJoinCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			printf("Chan create ID: %d\n", response.channel_info.channel_id);
			//:CHC!~CHC@unaffiliated/chc JOIN #aaa

			std::ostringstream s;
			ChatClientInfo client_info = irc_peer->getClientInfo();
			s << ":" << client_info.name << "!" << client_info.user << "@" << client_info.hostname << " JOIN " <<  response.channel_info.name << std::endl;
			irc_peer->SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
		}
}