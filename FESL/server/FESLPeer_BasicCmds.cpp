#include "FESLPeer.h"
#include "FESLDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/Auth.h>

#include <sstream>

namespace FESL {
	
	bool Peer::m_fsys_hello_handler(OS::KVReader kv_list) {
		/*
		TXN = Hello
		domainPartition.domain = eagames
		messengerIp = messaging.ea.com
		messengerPort = 13505
		domainPartition.subDomain = bf2142
		activityTimeoutSecs = 0
		curTime = "Nov-18-  17 02%3a19%3a07 UTC"
		theaterIp = bf2142 - pc.theater.ea.com
		theaterPort = 18305
		*/
		std::string kv_str = "TXN=Hello\ndomainPartition.domain=eagames\nmessengerIp=messaging.ea.com\nmessengerPort=13505\ndomainPartition.subDomain=bf2142\nactivityTimeoutSecs=0\ncurTime=\"Nov-18-  17 02%3a19%3a07 UTC\"\ntheaterIp=bf2142-pc.theater.ea.com\ntheaterPort=18305\n";
		SendPacket(FESL_TYPE_FSYS, kv_str);
		return true;
	}
	bool Peer::m_fsys_ping_handler(OS::KVReader kv_list) {
		return true;
	}
	bool Peer::m_fsys_goodbye_handler(OS::KVReader kv_list) {
		m_delete_flag = true;
		return true;
	}
	bool Peer::m_fsys_memcheck_handler(OS::KVReader kv_list) {
		//send_memcheck(0);
		return true;
	}
	
	bool Peer::m_acct_register_game_handler(OS::KVReader kv_list) {
		std::string kv_str = "TXN=RegisterGame\n";
		SendPacket(FESL_TYPE_ACCOUNT, kv_str);
		return true;
	}
	bool Peer::m_acct_login_handler(OS::KVReader kv_list) {
		std::string nick, password;
		if (kv_list.HasKey("encryptedInfo")) {
			kv_list = ((FESL::Driver *)this->GetDriver())->decryptString(kv_list.GetValue("encryptedInfo"));			
		}

		nick = kv_list.GetValue("name");
		password = kv_list.GetValue("password");

		if (kv_list.GetValueInt("returnEncryptedInfo")) {
			std::ostringstream s;
			s << "\\name\\" << nick;
			s << "\\password\\" << password;
			m_encrypted_login_info = ((FESL::Driver *)this->GetDriver())->encryptString(s.str());
		}
		OS::AuthTask::TryAuthUniqueNick_Plain(nick, OS_EA_PARTNER_CODE, FESL_ACCOUNT_NAMESPACEID, password, m_login_auth_cb, NULL, 0, this);
		return true;
	}
	void Peer::handle_auth_callback_error(OS::AuthResponseCode response_code, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		FESL_ERROR error = FESL_ERROR_AUTH_FAILURE;
		switch (response_code) {
		case OS::CREATE_RESPONE_UNIQUENICK_IN_USE:
			error = FESL_ERROR_ACCOUNT_EXISTS;
			break;
		case OS::LOGIN_RESPONSE_USER_NOT_FOUND:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		case OS::LOGIN_RESPONSE_INVALID_PROFILE:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		default:
		case OS::LOGIN_RESPONSE_SERVERINITFAILED:
		case OS::LOGIN_RESPONSE_DB_ERROR:
		case OS::LOGIN_RESPONSE_SERVER_ERROR:
			break;
		}
		SendError(cmd_type, error, TXN);
	}
	void Peer::handle_profile_search_callback_error(OS::EProfileResponseType response_code, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		/*
		EProfileResponseType_Success,
		EProfileResponseType_GenericError,
		EProfileResponseType_BadNick,
		EProfileResponseType_Bad_OldNick,
		EProfileResponseType_UniqueNick_Invalid,
		EProfileResponseType_UniqueNick_InUse,
		*/
		FESL_ERROR error = FESL_ERROR_AUTH_FAILURE;
		switch (response_code) {
			default:
			case OS::EProfileResponseType_UniqueNick_Invalid:
			case OS::EProfileResponseType_Bad_OldNick:
			case OS::EProfileResponseType_BadNick:
				SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name was invalid. Please change and try again.");
				return;
			case OS::EProfileResponseType_UniqueNick_InUse:
				//SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name is in use. Please choose another name.");
				error = FESL_ERROR_ACCOUNT_EXISTS;
				break;
			case OS::EProfileResponseType_GenericError:
				error = FESL_ERROR_SYSTEM_ERROR;
			break;
		}
		SendError(cmd_type, error, TXN);
		
	}
	void Peer::m_login_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=Login\n";
			s << "lkey=" << auth_data.session_key << "\n";
			((Peer *)peer)->m_session_key = auth_data.session_key;
			s << "displayName=" << profile.uniquenick << "\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			if (((Peer *)peer)->m_encrypted_login_info.length()) {
				s << "encryptedLoginInfo=" << ((Peer *)peer)->m_encrypted_login_info << "\n";
			}
			((Peer *)peer)->m_logged_in = true;
			((Peer *)peer)->m_user = user;
			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

			OS::ProfileSearchRequest request;
			request.type = OS::EProfileSearch_Profiles;
			request.user_search_details.id = user.id;
			request.profile_search_details.namespaceid = FESL_PROFILE_NAMESPACEID;
			request.namespaceids.push_back(request.profile_search_details.namespaceid);

			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			OS::m_profile_search_task_pool->AddRequest(request);
		}
		else {
			((Peer *)peer)->handle_auth_callback_error(auth_data.response_code, FESL_TYPE_ACCOUNT, "Login");
		}
	}
	bool Peer::m_acct_login_sub_account(OS::KVReader kv_list) {
		loginToSubAccount(kv_list.GetValue("name"));
		return true;
	}
	bool Peer::m_acct_gamespy_preauth(OS::KVReader kv_list) {
		OS::AuthTask::TryMakeAuthTicket(m_profile.id, m_create_auth_ticket, NULL, 0, this);
		return true;
	}
	bool Peer::m_acct_send_account_name(OS::KVReader kv_list) {
		return true;
	}
	bool Peer::m_acct_send_account_password(OS::KVReader kv_list) {
		return true;
	}
}