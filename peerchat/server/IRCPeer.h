#ifndef _IRCPEER_H
#define _IRCPEER_H
#include "ChatPeer.h"
#include "ChatBackend.h"

#include "../main.h"
#include <string>

#define MAX_OUTGOING_REQUEST_SIZE 1024

namespace Chat {
	class Driver;
	class IRCPeer;

	enum EIRCCommandHandlerRet {
		EIRCCommandHandlerRet_NoError,
		EIRCCommandHandlerRet_Unknown,
		EIRCCommandHandlerRet_NoPerms,
		EIRCCommandHandlerRet_NotEnoughParams,
	};

	typedef struct {
		std::string command;
		EIRCCommandHandlerRet (IRCPeer::*mpFunc)(std::vector<std::string> params, std::string full_params);
	} IRCCommandHandler;

	typedef struct {
		Chat::Driver *driver;
		std::string message;
		EChatMessageType message_type;
	} IRCMessageCallbackData;

	typedef struct {
		Chat::Driver *driver;
		std::string *search_data;
		std::string *target_user;
		uint32_t response_identifier;
	} GetCKeyData;

	typedef struct {
		Chat::Driver *driver;
		bool initial_join;
	} NamesCmdCBInfo;

	typedef struct {
		Chat::Driver *driver;
		std::string *message;
	} IRCCBDriverMessageCombo;


	class IRCPeer : public Peer {
	public:
		IRCPeer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~IRCPeer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved

		void OnRecvClientMessage(ChatClientInfo from_user, const char *msg, EChatMessageType message_type);
		void OnRecvChannelMessage(ChatClientInfo from_user, ChatChannelInfo to_channel, const char *msg, EChatMessageType message_type);
		void OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel);
		void OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel, EChannelPartTypes part_reason, std::string reason_str);
		void OnRecvChannelModeUpdate(ChatClientInfo user, ChatChannelInfo channel, ChanModeChangeData change_data);
		void OnChannelTopicUpdate(ChatClientInfo user, ChatChannelInfo channel);
		void OnSendSetChannelClientKeys(ChatClientInfo client, ChatChannelInfo channel, std::map<std::string, std::string> kv_data);
		void OnSendSetChannelKeys(ChatClientInfo client, ChatChannelInfo channel, const std::map<std::string, std::string> kv_data);
		void OnUserQuit(ChatClientInfo client, std::string quit_reason);
	protected:
		//user cmds
		EIRCCommandHandlerRet handle_nick(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_user(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_ping(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_pong(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_whois(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_userhost(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_privmsg(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_names(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_mode(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_topic(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_setkey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_getkey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_setchankey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_getchankey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_setckey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_getckey(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_quit(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_kick(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_oper(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_login(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_listusermodes(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_setusermode(std::vector<std::string> params, std::string full_params);

		//user cmd callbacks
		static void OnNickCmd_InUseLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnWhoisCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);		
		static void OnNickCmd_SubmitClientInfo(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnUserhostCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);		
		static void OnPrivMsg_Lookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnModeCmd_ShowChannelModes(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnGetKey_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnSetUserMode_SetUserMode(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);

		//channel cmds
		EIRCCommandHandlerRet handle_join(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_part(std::vector<std::string> params, std::string full_params);

		//channel cmd callbacks
		static void OnJoinCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnPartCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnNamesCmd_FindUsersCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnNamesCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnModeCmd_ChannelUpdateCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnTopicCmd_ChannelUpdateCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnGetCKeyCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnGetCKeyCmd_FindChanUserCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnSetChanKey_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnGetChanKey_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnListUserModesCmd_GetUserModes(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);

		//auth callbacks
		static void m_nick_email_auth_oper_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id);
		static void OnOperCmd_GetOperFlags(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);

		//channel misc
		void send_channel_topic(ChatChannelInfo channel);
		void send_channel_modes(ChatChannelInfo channel);
		void send_channel_names(ChatChannelInfo channel);
		bool send_callback_error(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response);
		void parse_channel_modes(std::string mode_str, uint32_t &add_mask, uint32_t &remove_mask, std::back_insert_iterator<std::vector<char> > invalid_modes, std::vector<std::string> params, std::string &password, int &limit, std::back_insert_iterator<std::vector<std::pair<std::string, ChanClientModeChange> > > user_modechanges);

		void send_nonick_channel_error(std::string name);
		void send_command_error(EIRCCommandHandlerRet value, std::string cmd_name);

		void send_client_init();

		void send_numeric(int num, std::string str, bool no_colon = false, std::string target_name = "");
		void SendPacket(const uint8_t *buff, int len);

		bool m_sent_client_init;

		static IRCCommandHandler mp_command_handler[];

		bool is_channel_name(std::string name);

		std::string m_quit_reason;

		int m_default_partnercode;


	};
}
#endif //_SAMPRAKPEER_H