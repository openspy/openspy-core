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
#include <iterator>
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
	1. implement /names - XXX no invisible
	2. implement channel modes - XXX no +k/+l
	3. implement user chan modes
	3. implement /part - XXX part reasons
	4. implement user modes
	5. implement channel/user keys XXX set/get chankey
	6. implement /notice /atm /utm, block ctcp - XXX BLOCK CTCP
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
			ChatBackendTask::SubmitAddUserToChannel(NULL, peer, driver, response.channel_info);
		}
		EIRCCommandHandlerRet IRCPeer::handle_join(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = OS::strip_whitespace(params[1]);
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
				send_channel_names(channel);
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
			ChatBackendTask::SubmitFindChannel(OnModeCmd_ShowChannelModes, this, mp_driver, channel.name);
		}
		void IRCPeer::send_channel_names(ChatChannelInfo channel) {
			ChatBackendTask::SubmitFindChannel(OnNamesCmd_FindChannelCallback, this, mp_driver, channel.name);
		}

		void IRCPeer::OnNamesCmd_FindUsersCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;


			if(!driver->HasPeer(peer)) {
				return;
			}

			std::ostringstream s;
			s << "= " << request.query_data.channel_info.name << " :";

			//TODO: split into multiple lines
			std::vector<ChatChanClientInfo>::const_iterator it = response.m_channel_clients.begin();
			while(it != response.m_channel_clients.end()) {
				const ChatChanClientInfo chan_client_info = *it;
				s << chan_client_info.client_info.name;
				it++;
				if(it != response.m_channel_clients.end()) {
					s << " ";
				}
			}

			irc_peer->send_numeric(353, s.str(), true);
			irc_peer->send_numeric(366, "End of /NAMES list.");


		}
		void IRCPeer::OnNamesCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}

			ChatBackendTask::getQueryTask()->flagPushTask();

			if(response.channel_info.channel_id == 0) {

			} else {
				ChatBackendTask::getQueryTask()->flagPushTask();
				ChatBackendTask::SubmitGetChannelUsers(OnNamesCmd_FindUsersCallback, irc_peer, driver, response.channel_info);
			}
		}

		EIRCCommandHandlerRet IRCPeer::handle_names(std::vector<std::string> params, std::string full_params) {
			std::string channel;
			if(params.size() >= 1) {
				channel = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			ChatBackendTask::SubmitFindChannel(OnNamesCmd_FindChannelCallback, this, mp_driver, channel);
			return EIRCCommandHandlerRet_NoError;
		}

		void IRCPeer::parse_channel_modes(std::string mode_str, uint32_t &add_mask, uint32_t &remove_mask, std::back_insert_iterator<std::vector<char> > it) {
			bool add = true;
			uint32_t flag;
			for(int i=0;i<mode_str.length();i++) {
				flag = 0;
				switch(mode_str[i]) {
					case '-':
						add = false;
						break;
					case '+':
						add = true;
						break;
					case 'p':
						flag = EChannelModeFlags_Private;
					break;
					case 's':
						flag = EChannelModeFlags_Secret;
					break;
					case 'u':
						flag = EChannelModeFlags_Auditormium;
					break;
					case 'q':
						flag = EChannelModeFlags_Auditormium;
					break;
					case 'n':
						flag = EChannelModeFlags_NoOutsideMsgs;
					break;
					case 'i':
						flag = EChannelModeFlags_InviteOnly;
					break;
					case 'Z':
						flag = EChannelModeFlags_OnlyOwner;
					break;
					case 'r':
						flag = EChannelModeFlags_StayOpened;
					break;
					case 'm':
						flag = EChannelModeFlags_Moderated;
					break;
					default:
						*(it++) = mode_str[i];
					break;
				}
				if(add) {
					add_mask |= flag;
				} else {
					remove_mask |= flag;
				}
			}
		}
		void IRCPeer::OnRecvChannelModeUpdate(ChatClientInfo user, ChatChannelInfo channel, ChanModeChangeData change_data) {
			int old_modeflags = change_data.old_modeflags;

			int set_flags = (old_modeflags ^ channel.modeflags) & ~old_modeflags;
			int unset_flags = (old_modeflags & ~channel.modeflags) & old_modeflags;

			std::ostringstream mode_add_str;
			std::ostringstream mode_str;
			mode_add_str << "+";
			if(set_flags & EChannelModeFlags_Private) {
				mode_add_str << "p";
			}
			if(set_flags & EChannelModeFlags_Secret) {
				mode_add_str << "s";
			}
			if(set_flags & EChannelModeFlags_InviteOnly) {
				mode_add_str << "i";
			}
			if(set_flags & EChannelModeFlags_TopicOpOnly) {
				mode_add_str << "t";
			}
			if(set_flags & EChannelModeFlags_NoOutsideMsgs) {
				mode_add_str << "n";
			}
			if(set_flags & EChannelModeFlags_Moderated) {
				mode_add_str << "m";
			}
			if(set_flags & EChannelModeFlags_Auditormium) {
				mode_add_str << "u";
			}
			if(set_flags & EChannelModeFlags_VOPAuditormium) {
				mode_add_str << "q";
			}
			if(set_flags & EChannelModeFlags_StayOpened) {
				mode_add_str << "r";
			}
			if(set_flags & EChannelModeFlags_OnlyOwner) {
				mode_add_str << "Z";
			}

			std::ostringstream mode_del_str;
			mode_del_str << "-";
			if(unset_flags & EChannelModeFlags_Private) {
				mode_del_str << "p";
			}
			if(unset_flags & EChannelModeFlags_Secret) {
				mode_del_str << "s";
			}
			if(unset_flags & EChannelModeFlags_InviteOnly) {
				mode_del_str << "i";
			}
			if(unset_flags & EChannelModeFlags_TopicOpOnly) {
				mode_del_str << "t";
			}
			if(unset_flags & EChannelModeFlags_NoOutsideMsgs) {
				mode_del_str << "n";
			}
			if(unset_flags & EChannelModeFlags_Moderated) {
				mode_del_str << "m";
			}
			if(unset_flags & EChannelModeFlags_Auditormium) {
				mode_del_str << "u";
			}
			if(unset_flags & EChannelModeFlags_VOPAuditormium) {
				mode_del_str << "q";
			}
			if(unset_flags & EChannelModeFlags_StayOpened) {
				mode_del_str << "r";
			}
			if(unset_flags & EChannelModeFlags_OnlyOwner) {
				mode_del_str << "Z";
			}

			if(mode_add_str.str().length() > 1) {
				mode_str << mode_add_str.str();
			}
			if(mode_del_str.str().length() > 1) {
				mode_str << mode_del_str.str();
			}
			std::ostringstream s;

			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " MODE " << channel.name << " " << mode_str.str() << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
		}
		void IRCPeer::OnChannelTopicUpdate(ChatClientInfo user, ChatChannelInfo channel) {
			std::ostringstream s;
			s << ":" << user.name << "!" << user.user << "@" << user.hostname << " TOPIC " << channel.name << " :" << channel.topic << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
		}
		void IRCPeer::OnModeCmd_ChannelUpdateCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
			}
		}
		void IRCPeer::OnModeCmd_ShowChannelModes(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}
			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				return;
			}
			int set_flags = response.channel_info.modeflags;
			std::ostringstream mode_add_str, mode_str;
			mode_add_str << "+";
			if(set_flags & EChannelModeFlags_Private) {
				mode_add_str << "p";
			}
			if(set_flags & EChannelModeFlags_Secret) {
				mode_add_str << "s";
			}
			if(set_flags & EChannelModeFlags_InviteOnly) {
				mode_add_str << "i";
			}
			if(set_flags & EChannelModeFlags_TopicOpOnly) {
				mode_add_str << "t";
			}
			if(set_flags & EChannelModeFlags_NoOutsideMsgs) {
				mode_add_str << "n";
			}
			if(set_flags & EChannelModeFlags_Moderated) {
				mode_add_str << "m";
			}
			if(set_flags & EChannelModeFlags_Auditormium) {
				mode_add_str << "u";
			}
			if(set_flags & EChannelModeFlags_VOPAuditormium) {
				mode_add_str << "q";
			}
			if(set_flags & EChannelModeFlags_StayOpened) {
				mode_add_str << "r";
			}
			if(set_flags & EChannelModeFlags_OnlyOwner) {
				mode_add_str << "Z";
			}
			mode_str << response.channel_info.name << " " << mode_add_str.str();
			irc_peer->send_numeric(324, mode_str.str(), true);
		}
		EIRCCommandHandlerRet IRCPeer::handle_mode(std::vector<std::string> params, std::string full_params) {
			std::string target, mode_str;
			if(params.size() > 2) {
				target = params[1];
				mode_str = params[2];
			} else {
				target = params[1];
				if(params.size()> 1) { //return chan or user modes
					if(is_channel_name(target)) {
						ChatBackendTask::SubmitFindChannel(OnModeCmd_ShowChannelModes, this, mp_driver, target);
					} else {
					}
				}
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			uint32_t addmask = 0, removemask = 0;
			ChatChannelInfo channel;
			if(is_channel_name(target)) {
				std::vector<char> bad_modes;
				parse_channel_modes(params[2], addmask, removemask, std::back_inserter(bad_modes));
				channel.name = target;
				channel.channel_id = 0;
				ChatBackendTask::SubmitUpdateChannelModes(OnModeCmd_ChannelUpdateCallback, this, mp_driver, addmask, removemask, channel);
			} else {
			}
			return EIRCCommandHandlerRet_NoError;	
		}
		void IRCPeer::OnTopicCmd_ChannelUpdateCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			Chat::Driver *driver = (Chat::Driver *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;

			if(!driver->HasPeer(peer)) {
				return;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				return;
			}
		}
		EIRCCommandHandlerRet IRCPeer::handle_topic(std::vector<std::string> params, std::string full_params) {
			std::string target, topic;
			ChatChannelInfo channel;
			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(params.size() > 2 && beg) {
				target = params[1];
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}

			topic = (const char *)(beg+1);

			if(is_channel_name(target)) {
				channel.name = target;
				channel.channel_id = 0;
				ChatBackendTask::SubmitUpdateChannelTopic(OnTopicCmd_ChannelUpdateCallback, this, mp_driver, channel, topic);
			} else {
				//send error isn't channel
				//void send_numeric(int num, std::string str, bool no_colon = false, std::string target_name = "");
			}
			return EIRCCommandHandlerRet_NoError;	
		}
		void IRCPeer::OnSendSetChannelClientKeys(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data) {
			std::ostringstream s;
			std::map<std::string, std::string>::iterator it = kv_data.begin();
			//:s 702 #test #test CHC BCAST :\b_test\666
			s << channel.name  << " BCAST :";
			int num_broadcasts = 0;
			while(it != kv_data.end()) {
				std::pair<std::string, std::string> p = *it;
				if(strncmp(p.first.c_str(), "b_", 2) == 0) {
					num_broadcasts++;
					s << "\\" << p.first << "\\" << p.second;
				}				
				it++;
			}
			if(num_broadcasts > 0)
				send_numeric(704, s.str(), true, channel.name);
		}
		EIRCCommandHandlerRet IRCPeer::handle_setckey(std::vector<std::string> params, std::string full_params) {
			//setckey #test CHC 0 061 :\b_rating\666
			std::string channel, target_user, unk1, unk2, set_data;

			std::map<std::string, std::string> set_data_map;
			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			if(params.size() > 5 && beg && strlen(beg) >= 2 && beg[1] == '\\') {
				channel = params[1];
				target_user = params[2];
				unk1 = params[3];
				unk2 = params[4];
				set_data = (beg+2);
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;	
			}
			int i = 0;
			char *set_data_cpy = strdup(set_data.c_str());
			char data[256];
			std::string key, value;
			while(find_param(i++, set_data_cpy, data, sizeof(data))) {
				if((i % 2)) {
					key = data;
				} else {
					value = data;
				}
				if(value.length()) {
					set_data_map[key] = value;
					value = "";
					key = "";
				}
			}
			ChatBackendTask::SubmitSetChannelKeys(NULL, this, mp_driver, channel, target_user, set_data_map);
			free((void *)set_data_cpy);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnGetCKeyCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			GetCKeyData *cb_data = (GetCKeyData *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;

			if(!driver->HasPeer(peer)) {
				delete cb_data->search_data;
				delete cb_data->target_user;
				free((void *)cb_data);
				return;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				delete cb_data->search_data;
				delete cb_data->target_user;
				free((void *)cb_data);
				return;
			}
			ChatBackendTask::getQueryTask()->flagPushTask();
			ChatBackendTask::SubmitGetChannelUser(OnGetCKeyCmd_FindChanUserCallback, irc_peer, cb_data, response.channel_info, *cb_data->target_user);
		}
		//TODO: send multiple users getckeys
		void IRCPeer::OnGetCKeyCmd_FindChanUserCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			GetCKeyData *cb_data = (GetCKeyData *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;
			std::string *search_params = (std::string *)extra;

			char *search_data_cpy;
			char key_name[256];
			int i = 0;

			std::ostringstream result_oss, end_oss;
			
			std::ostringstream s;

			std::map<std::string, std::string>::const_iterator it;

			if(!driver->HasPeer(peer)) {
				goto end_cleanup;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				goto end_cleanup;
			}
			search_data_cpy = strdup(cb_data->search_data->c_str());
			
			while(find_param(i++, search_data_cpy, key_name, sizeof(key_name))) {
					it = response.chan_client_info.custom_keys.find(key_name);
					result_oss << "\\";
					if(it != response.chan_client_info.custom_keys.end()) {
						result_oss << (*it).second;
					}
			}
			s << irc_peer->m_client_info.name << " " << response.channel_info.name << " " << response.client_info.name << " " << cb_data->response_identifier << " :" << result_oss.str();

			irc_peer->send_numeric(702, s.str(), true);
			s.str("");
			result_oss.str("");

			end_oss << irc_peer->m_client_info.name << " " << response.channel_info.name << " " << cb_data->response_identifier << " :End of GETCKEY";
			irc_peer->send_numeric(703, end_oss.str(), true);

			free((void *)search_data_cpy);

			end_cleanup:
			delete cb_data->search_data;
			delete cb_data->target_user;
			free((void *)cb_data);
		}
		EIRCCommandHandlerRet IRCPeer::handle_getckey(std::vector<std::string> params, std::string full_params) {
			const char *str = full_params.c_str();
			const char *beg = strchr(str, ':');
			GetCKeyData *cb_data;
			std::string channel_name, target_name;
			if(params.size() > 2 && beg) {
				channel_name = params[1];
				target_name = params[2];
				beg++;
			} else {
				return EIRCCommandHandlerRet_NotEnoughParams;
			}
			cb_data = (GetCKeyData *)malloc(sizeof(GetCKeyData));
			cb_data->driver = mp_driver;
			cb_data->search_data = new std::string(OS::strip_whitespace(beg));
			cb_data->target_user = new std::string(target_name);
			cb_data->response_identifier = atoi(params[3].c_str());

			ChatBackendTask::SubmitFindChannel(OnGetCKeyCmd_FindChannelCallback, this, cb_data, channel_name);
			return EIRCCommandHandlerRet_NoError;	
		}
		void IRCPeer::OnSetChanKey_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			GetCKeyData *cb_data = (GetCKeyData *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;
			std::string *search_params = (std::string *)extra;

			if(!driver->HasPeer(peer)) {
				delete cb_data->search_data;
				delete cb_data->target_user;
				free((void *)cb_data);
				return;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				delete cb_data->search_data;
				delete cb_data->target_user;
				free((void *)cb_data);
				return;
			}

			ChatBackendTask::SubmitSetChannelKeys(NULL, peer, driver, response.channel_info, OS::KeyStringToMap(*(cb_data->search_data)));

			delete cb_data->search_data;
			delete cb_data->target_user;
			free((void *)cb_data);

		}

		void IRCPeer::OnSendSetChannelKeys(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data) {
			std::ostringstream s;
			std::map<std::string, std::string>::const_iterator it = kv_data.begin();
			//:s 702 #test #test CHC BCAST :\b_test\666
			s << channel.name << " " << client.name << " BCAST :";
			int num_broadcasts = 0;
			while(it != kv_data.end()) {
				const std::pair<std::string, std::string> p = *it;
				if(strncmp(p.first.c_str(), "b_", 2) == 0) {
					num_broadcasts++;
					s << "\\" << p.first << "\\" << p.second;
				}				
				it++;
			}
			if(num_broadcasts > 0)
				send_numeric(702, s.str(), true, channel.name);
		}
		/*
			-> s SETCHANKEY #test 0 000 :\b_test\666
			<- :s 704 #test #test BCAST :\b_test\666
		*/
		EIRCCommandHandlerRet IRCPeer::handle_setchankey(std::vector<std::string> params, std::string full_params) {
			std::string target, set_data_str;
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

			target = params[1];

			cb_data = (GetCKeyData *)malloc(sizeof(GetCKeyData));
			cb_data->driver = mp_driver;
			cb_data->search_data = new std::string(OS::strip_whitespace(beg));
			cb_data->target_user = new std::string(target);
			cb_data->response_identifier = atoi(params[2].c_str());


			ChatBackendTask::SubmitFindChannel(OnSetChanKey_FindChannelCallback, this, cb_data, target);
			return EIRCCommandHandlerRet_NoError;
		}
		void IRCPeer::OnGetChanKey_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra) {
			GetCKeyData *cb_data = (GetCKeyData *)extra;
			IRCPeer *irc_peer = (IRCPeer *)peer;
			Chat::Driver *driver = (Chat::Driver *)cb_data->driver;
			std::string *search_params = (std::string *)extra;

			char *search_data_cpy;
			char key_name[256];
			int i = 0;

			std::ostringstream result_oss, end_oss;
			
			std::ostringstream s;

			std::map<std::string, std::string>::const_iterator it;

			if(!driver->HasPeer(peer)) {
				goto end_cleanup;
			}

			if(response.error != EChatBackendResponseError_NoError) {
				irc_peer->send_callback_error(request, response);
				goto end_cleanup;
			}

			search_data_cpy = strdup(cb_data->search_data->c_str());

			while(find_param(i++, search_data_cpy, key_name, sizeof(key_name))) {
					it = response.channel_info.custom_keys.find(key_name);
					result_oss << "\\";
					if(it != response.channel_info.custom_keys.end()) {
						result_oss << (*it).second;
					}
			}

			s << irc_peer->m_client_info.name << " " << response.channel_info.name << " " << cb_data->response_identifier << " :" << result_oss.str();

			irc_peer->send_numeric(704, s.str(), true);

			end_cleanup:
			delete cb_data->search_data;
			delete cb_data->target_user;
			free((void *)cb_data);
		}
		/*
			-> s GETCHANKEY #test 0 000 :\b_test
			<- :s 704 CHC #test 0 :\666
		*/
		EIRCCommandHandlerRet IRCPeer::handle_getchankey(std::vector<std::string> params, std::string full_params) {
			std::string target, set_data_str;
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
			target = params[1];

			cb_data = (GetCKeyData *)malloc(sizeof(GetCKeyData));
			cb_data->driver = mp_driver;
			cb_data->search_data = new std::string(OS::strip_whitespace(beg));
			cb_data->target_user = new std::string(target);
			cb_data->response_identifier = atoi(params[2].c_str());


			ChatBackendTask::SubmitFindChannel(OnGetChanKey_FindChannelCallback, this, cb_data, target);
			return EIRCCommandHandlerRet_NoError;
		}
}