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

#include <OS/Auth.h>

namespace Chat {
		void IRCPeer::OnNickCmd_InUseLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			irc_peer->mp_mutex->lock();

			if(response.client_info.name.size() > 0) {
				std::ostringstream s;
				s << response.client_info.name << " :Nickname is already in use";
				irc_peer->send_numeric(433, s.str(), true);
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
			s << ":" << ((ChatServer*)mp_driver->getServer())->getName() << " ";
			if(params.size() > 1) {
				s << "PONG " << ((ChatServer*)mp_driver->getServer())->getName() << " :" << params[1] << std::endl;
			} else {
				s << "PONG " << ((ChatServer*)mp_driver->getServer())->getName() << std::endl;
			}
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
			return EIRCCommandHandlerRet_NoError;	
		}
		EIRCCommandHandlerRet IRCPeer::handle_pong(std::vector<std::string> params, std::string full_params) {
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnPrivMsg_Lookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			IRCMessageCallbackData *cb_data = (IRCMessageCallbackData *)extra;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			std::ostringstream s;

			if(!driver->HasPeer(peer)) {
				goto end_cleanup;
			}
			if(irc_peer->send_callback_error(request, response)) {
				goto end_cleanup;
			}
			if(response.client_info.client_id != 0) {
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitClientMessage(response.client_info.client_id, cb_data->message, cb_data->message_type, NULL, peer, driver);
			} else if(response.channel_info.channel_id != 0) {
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitChannelMessage(response.channel_info.channel_id, cb_data->message, cb_data->message_type, NULL, peer, driver);
			}
			end_cleanup:
			delete cb_data;
		}
		EIRCCommandHandlerRet IRCPeer::handle_privmsg(std::vector<std::string> params, std::string full_params) {
			std::string target;
			EChatMessageType type;
			if(params.size() > 2) {
				target = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(beg) {
				beg++;
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			IRCMessageCallbackData *cb_data = new IRCMessageCallbackData;

			if(strcasecmp(params[0].c_str(),"PRIVMSG") == 0) {
				type = EChatMessageType_Msg;
			} else if(strcasecmp(params[0].c_str(),"NOTICE") == 0) {
				type = EChatMessageType_Notice;
			} else if(strcasecmp(params[0].c_str(),"UTM") == 0) {
				type = EChatMessageType_UTM;
			} else if(strcasecmp(params[0].c_str(),"ATM") == 0) {
				type = EChatMessageType_ATM;
			}
			cb_data->message = beg;
			cb_data->driver = mp_driver;
			cb_data->message_type = type;

			if(is_channel_name(target)) {
				ChatBackendTask::SubmitFindChannel(OnPrivMsg_Lookup, (Peer *)this, cb_data, target);
			} else {
				ChatBackendTask::SubmitGetClientInfoByName(target, OnPrivMsg_Lookup, (Peer *)this, cb_data);
				
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
			if(irc_peer->send_callback_error(request, response)) {
				return;
			}
			if(response.client_info.name.size() > 0) {
				s.str("");
				s << response.client_info.name << " " << response.client_info.user  << " " << response.client_info.hostname << " * :" << response.client_info.realname;
				irc_peer->send_numeric(311, s.str(), true);
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
			if(irc_peer->send_callback_error(request, response)) {
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

		EIRCCommandHandlerRet IRCPeer::handle_setkey(std::vector<std::string> params, std::string full_params) {
			std::string target, identifier, set_data_str;
			if(params.size() < 5) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(beg) {
				beg++;
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			ChatBackendTask::SubmitSetClientKeys(NULL, this, mp_driver, m_client_info.client_id, OS::KeyStringToMap(beg));
			//SubmitSetChannelKeys

			return EIRCCommandHandlerRet_NoError;
		}
		/*
			-> s GETKEY Krad 0 000 :\gameid
			<- :s 700 CHC Krad 0 :\0
		*/
		void IRCPeer::OnGetKey_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			GetCKeyData *cb_data = (GetCKeyData *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;

			std::map<std::string, std::string>::const_iterator it;

			char *search_data_cpy = NULL;
			char key_name[256];
			int i = 0;

			std::ostringstream s;
			std::ostringstream result_oss;

			if(!driver->HasPeer(peer)) {
				goto end_error;
			}
			if(irc_peer->send_callback_error(request, response)) {
				goto end_error;
			}

			search_data_cpy = strdup(cb_data->search_data->c_str());
			while(find_param(i++, search_data_cpy, key_name, sizeof(key_name))) {
					it = response.client_info.custom_keys.find(key_name);
					result_oss << "\\";
					//TODO - special attributes: gameid, user, nick, realname, profileid, userid
					if(it != response.client_info.custom_keys.end()) {
						result_oss << (*it).second;
					}
			}
			s << irc_peer->m_client_info.name  << " " << response.client_info.name << " " << cb_data->response_identifier << " :" << result_oss.str();
			irc_peer->send_numeric(700, s.str(), true);

			end_error:

			if(search_data_cpy)
				free((void *)search_data_cpy);
			delete cb_data->search_data;
			delete cb_data->target_user;
			free((void *)cb_data);
		}
		EIRCCommandHandlerRet IRCPeer::handle_getkey(std::vector<std::string> params, std::string full_params) {
			std::string target, identifier, set_data_str;
			GetCKeyData *cb_data;
			if(params.size() < 5) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(beg) {
				beg++;
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			cb_data = (GetCKeyData *)malloc(sizeof(GetCKeyData));
			cb_data->driver = mp_driver;
			cb_data->search_data = new std::string(OS::strip_whitespace(beg));
			cb_data->target_user = new std::string(params[1]);
			cb_data->response_identifier = atoi(params[3].c_str());

			ChatBackendTask::SubmitGetClientInfoByName(params[1], OnGetKey_UserLookup, (Peer *)this, cb_data);

			return EIRCCommandHandlerRet_NoError;
		}
		EIRCCommandHandlerRet IRCPeer::handle_quit(std::vector<std::string> params, std::string full_params) {
			const char *reason = strchr(full_params.c_str(), ':');

			if(reason) {
				m_quit_reason = (reason+1);
				m_delete_flag = true;
				m_timeout_flag = false;
			}
			return EIRCCommandHandlerRet_NoError;
		}

		void IRCPeer::OnOperCmd_GetOperFlags(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)extra;
			std::ostringstream s;
			if(!driver->HasPeer(peer)) {
				return;
			}
			if(irc_peer->send_callback_error(request, response)) {
				return;
			}
			
			s << ":SERVER!SERVER@* NOTICE " << irc_peer->m_client_info.name << " :Authenticated" << std::endl;
			irc_peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());

			if(response.operflags != 0) {
				s.str("");
				s << ":SERVER!SERVER@* NOTICE " << irc_peer->m_client_info.name << " :Rights Granted" << std::endl;
				irc_peer->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
				irc_peer->m_client_info.operflags = response.operflags;
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitClientInfo(NULL, (Peer *)irc_peer, driver);
			}
		}

		void IRCPeer::m_nick_email_auth_oper_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id) {
			ChatCallbackContext *cb_ctx = (ChatCallbackContext *)extra;

			IRCPeer *irc_peer = (IRCPeer *)cb_ctx->peer;
			Chat::Driver *driver = (Chat::Driver *)cb_ctx->driver;
			if(!driver->HasPeer(irc_peer)) {
				goto end_error;
			}

			/*
			if(irc_peer->send_callback_error(request, response)) {
				goto end_error;
			}
			*/
			

			if(success) {
				printf("Got profileid: %d\n", profile.id);	

				irc_peer->m_client_info.profileid = profile.id;				
				ChatBackendTask::SubmitGetChatOperFlags(profile.id, OnOperCmd_GetOperFlags, (Peer *)irc_peer, driver);
			} else {
				//inc counter, ban if over x attempts
			}

			
			end_error:
				delete cb_ctx;
		}

		EIRCCommandHandlerRet IRCPeer::handle_oper(std::vector<std::string> params, std::string full_params) {
			std::string email, pass, nick;

			if(params.size() < 3) {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			email = params[1];
			nick = params[2];
			pass = params[3];

			printf("Oper: %s - %s - %s\n",email.c_str(), nick.c_str(), pass.c_str());

			ChatCallbackContext *cb_ctx = new ChatCallbackContext;
			cb_ctx->peer = this;
			cb_ctx->driver = mp_driver;

			OS::AuthTask::TryAuthNickEmail(nick, email, m_default_partnercode, pass, false, m_nick_email_auth_oper_cb, cb_ctx, 0);

			return EIRCCommandHandlerRet_NoError;
		}
}