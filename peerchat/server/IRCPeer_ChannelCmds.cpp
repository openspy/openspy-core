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
/*
	1. implement /names
	2. implement channel modes
	3. implement user chan modes
	3. implement /part
	4. implement user modes
	5. implement channel/user keys
	6. implement /notice /atm /utm, block ctcp
	6. clean up on disconnect
	7. implement /setchanprops /setusetmode /get*
	8. implement auth
	9. implement crypt
	10. implement ssl
*/


namespace Chat {
		void IRCPeer::OnPartCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			printf("Send part channel: %d\n", response.channel_info.channel_id);

			ChatBackendTask::getQueryTask()->flagPushTask();
			ChatBackendTask::SubmitRemoveUserFromChannel(NULL, irc_peer, driver, response.channel_info);
		}
		EIRCCommandHandlerRet IRCPeer::handle_part(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFindChannel(OnPartCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnJoinCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {

			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			printf("Chan create ID: %d\n", response.channel_info.channel_id);


			ChatBackendTask::SubmitAddUserToChannel(NULL, peer, driver, response.channel_info);
		}
		EIRCCommandHandlerRet IRCPeer::handle_join(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFind_OrCreateChannel(OnJoinCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::send_channel_topic(ChatChannelInfo channel) {
			std::ostringstream s;
			s << channel.name << " :" << channel.topic;
			send_numeric(332, s.str(), true);

			s.str("");
			s << channel.name << " " << channel.topic_setby << " " << channel.topic_seton;
			send_numeric(333, s.str(), true);
		}
		void IRCPeer::OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel) {
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " JOIN " <<  channel.name << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());

			if(user.client_id == m_client_info.client_id) {
				m_channel_list.push_back(channel.channel_id);
				send_channel_topic(channel);
			}
		}
		void IRCPeer::OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel) {
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " PART " <<  channel.name << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());

			if(user.client_id == m_client_info.client_id) {
				std::vector<int>::iterator it = std::find(m_channel_list.begin(), m_channel_list.end(), 5);
				if(it != m_channel_list.end())
				    m_channel_list.erase(it);
			}
		}
		void IRCPeer::send_channel_modes(ChatChannelInfo channel) {
		}
		void IRCPeer::send_channel_names(ChatChannelInfo channel) {

		}




		EIRCCommandHandlerRet IRCPeer::handle_names(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			//ChatBackendTask::SubmitFindChannel(OnPartCmd_FindCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}
}