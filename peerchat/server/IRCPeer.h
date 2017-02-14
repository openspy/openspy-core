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
	} IRCMessageCallbackData;


	class IRCPeer : public Peer {
	public:
		IRCPeer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~IRCPeer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved

		void OnRecvClientMessage(ChatClientInfo from_user, const char *msg);
		void OnRecvClientJoinChannel(ChatClientInfo user, ChatChannelInfo channel);
		void OnRecvClientPartChannel(ChatClientInfo user, ChatChannelInfo channel);
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

		//user cmd callbacks
		static void OnNickCmd_InUseLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnWhoisCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);		
		static void OnNickCmd_SubmitClientInfo(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnUserhostCmd_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);		
		static void OnPrivMsg_UserLookup(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnNotice_Sent(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);

		//channel cmds
		EIRCCommandHandlerRet handle_join(std::vector<std::string> params, std::string full_params);
		EIRCCommandHandlerRet handle_part(std::vector<std::string> params, std::string full_params);

		//channel cmd callbacks
		static void OnJoinCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnPartCmd_FindCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnNamesCmd_FindUsersCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);
		static void OnNamesCmd_FindChannelCallback(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response, Peer *peer,void *extra);

		//channel misc
		void send_channel_topic(ChatChannelInfo channel);
		void send_channel_modes(ChatChannelInfo channel);
		void send_channel_names(ChatChannelInfo channel);

		void send_nonick_channel_error(std::string name);
		void send_command_error(EIRCCommandHandlerRet value, std::string cmd_name);

		void send_client_init();

		void send_numeric(int num, std::string str, bool no_colon = false);
		void SendPacket(const uint8_t *buff, int len);

		bool m_sent_client_init;

		static IRCCommandHandler mp_command_handler[];

		bool is_channel_name(std::string name);


	};
}
#endif //_SAMPRAKPEER_H