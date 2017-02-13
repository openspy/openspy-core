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
		void IRCPeer::OnNickCmd_InUseLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			irc_peer->mp_mutex->lock();

			if(response.client_info.name.size() > 0) {
				irc_peer->send_numeric(433, "Nickname is already in use");
			} else {
				irc_peer->m_client_info.name = request.query_name;
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitClientInfo(OnNickCmd_SubmitClientInfo, peer, driver);
			}
			irc_peer->mp_mutex->unlock();
		}
		void IRCPeer::OnNickCmd_SubmitClientInfo(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			printf("submitclientinfo callback\n");

			irc_peer->mp_mutex->lock();
			irc_peer->m_client_info = response.client_info;

			irc_peer->send_client_init();

			std::ostringstream s;
			s << "Your client id is " << irc_peer->m_client_info.client_id;
			irc_peer->send_numeric(401, s.str());

			irc_peer->mp_mutex->unlock();
		}
		EIRCCommandHandlerRet IRCPeer::handle_nick(std::vector<std::string> params, std::string full_params) {
			std::string nick;
			if(params.size() >= 1) {
				nick = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			if(nick.size() && nick[0] == ':') {
				nick = nick.substr(1, nick.length()-1);
			}
			nick = OS::strip_whitespace(nick);

			ChatBackendTask::SubmitGetClientInfoByName(nick, OnNickCmd_InUseLookup, (Peer *)this, mp_driver);

			return EIRCCommandHandlerRet_NoError;
		}
		EIRCCommandHandlerRet IRCPeer::handle_user(std::vector<std::string> params, std::string full_params) {
			if(params.size() < 5) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			m_client_info.user = params[1];
			m_client_info.realname = params[4];
			m_client_info.realname = m_client_info.realname.substr(1, m_client_info.realname.length()-1);

			if(m_client_info.name.size())
				send_client_init();

			return EIRCCommandHandlerRet_NoError;
		}
		EIRCCommandHandlerRet IRCPeer::handle_ping(std::vector<std::string> params, std::string full_params) {
			std::ostringstream s;
			s << "PONG :" << ((ChatServer*)mp_driver->getServer())->getName() << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
			return EIRCCommandHandlerRet_NoError;	
		}
		EIRCCommandHandlerRet IRCPeer::handle_pong(std::vector<std::string> params, std::string full_params) {
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnPrivMsg_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			IRCMessageCallbackData *cb_data = (IRCMessageCallbackData *)extra;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				delete cb_data;
				return;
			}
			std::ostringstream s;
			if(response.client_info.name.size() > 0) {
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitClientMessage(response.client_info.client_id, cb_data->message, NULL, peer, driver);
			} else {
				//
				irc_peer->send_nonick_channel_error(request.query_name);
			}
			delete cb_data;
		}
		EIRCCommandHandlerRet IRCPeer::handle_privmsg(std::vector<std::string> params, std::string full_params) {
			std::string target;
			if(params.size() > 1) {
				target = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			if(is_channel_name(target)) {
				//ChatBackendTask::SubmitChannelMessage(target, OnWhoisCmd_UserLookup, (Peer *)this, mp_driver);	
			} else {
				const char *str = full_params.c_str();
				const char *beg = strchr(str, ':');
				if(beg) {
					beg++;
				} else {
					return EIRCCommandHandlerRet_NotEnoughParams;		
				}

				IRCMessageCallbackData *cb_data = new IRCMessageCallbackData;
				cb_data->message = beg;
				cb_data->driver = mp_driver;
				ChatBackendTask::SubmitGetClientInfoByName(params[1], OnPrivMsg_UserLookup, (Peer *)this, cb_data);
				printf("Sending: %s\n", beg);
				
			}			
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnWhoisCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}
			std::ostringstream s;
			if(response.client_info.name.size() > 0) {
				s.str("");
				s << response.client_info.name << " " << response.client_info.user  << " " << response.client_info.hostname << " * :" << response.client_info.realname << std::endl;
				irc_peer->send_numeric(311, s.str(), true);
			} else {
				//
				irc_peer->send_nonick_channel_error(request.query_name);
			}

			s.str("");
			s << request.query_name << " :End of WHOIS list";
			irc_peer->send_numeric(318, s.str(), true);
		}
		EIRCCommandHandlerRet IRCPeer::handle_whois(std::vector<std::string> params, std::string full_params) {
			if(params.size() < 2) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitGetClientInfoByName(params[1], OnWhoisCmd_UserLookup, (Peer *)this, mp_driver);
			return EIRCCommandHandlerRet_NoError;		
		}
		void IRCPeer::OnUserhostCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}
			if(response.client_info.name.size() > 0) {
				std::ostringstream s;
				s << response.client_info.name << "=+" << response.client_info.user << "@" << response.client_info.hostname << std::endl;
				irc_peer->send_numeric(302, s.str());
			} else {
				//
				irc_peer->send_nonick_channel_error(request.query_name);
			}

		}
		EIRCCommandHandlerRet IRCPeer::handle_userhost(std::vector<std::string> params, std::string full_params) {
			if(params.size() < 2) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			std::string search = m_client_info.name;
			if(params.size() > 1) {
				search = params[1];
			}

			ChatBackendTask::SubmitGetClientInfoByName(search, OnUserhostCmd_UserLookup, (Peer *)this, mp_driver);
			return EIRCCommandHandlerRet_NoError;		
		}
}