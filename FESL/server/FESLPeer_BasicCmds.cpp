#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"


#include <sstream>


namespace FESL {
	
	bool Peer::m_fsys_hello_handler(OS::KVReader kv_list) {
		std::ostringstream ss;

		char timeBuff[128];
		struct tm *newtime;
		time_t long_time;
		time(&long_time);
		newtime = localtime(&long_time);

		strftime(timeBuff, sizeof(timeBuff), "%h-%e-%g %T %Z", newtime);

		PublicInfo public_info = ((FESL::Driver *)mp_driver)->GetServerInfo();
		ss << "TXN=Hello\n";
		ss << "domainPartition.domain=" << public_info.domainPartition << "\n";
		ss << "messengerIp=" << public_info.messagingHostname << "\n";
		ss << "messengerPort=" << public_info.messagingPort << "\n";
		ss << "domationPartition.subDomain=" << public_info.subDomain << "\n";
		ss << "activityTimeoutSecs=" << FESL_PING_TIME * 2 << "\n";
		ss << "curTime=\"" << OS::url_encode(timeBuff) << "\"\n";
		ss << "theaterIp=" << public_info.theaterHostname << "\n";
		ss << "theaterPort=" << public_info.theaterPort << "\n";
		SendPacket(FESL_TYPE_FSYS, ss.str());

		send_memcheck(0);
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
			m_encrypted_login_info = OS::url_decode(kv_list.GetValue("encryptedInfo"));
			kv_list = ((FESL::Driver *)this->GetDriver())->getStringCrypter()->decryptString(m_encrypted_login_info);
		}

		nick = kv_list.GetValue("name");
		password = kv_list.GetValue("password");

		if (kv_list.GetValueInt("returnEncryptedInfo")) {
			std::ostringstream s;
			s << "\\name\\" << nick;
			s << "\\password\\" << password;
			m_encrypted_login_info = ((FESL::Driver *)this->GetDriver())->getStringCrypter()->encryptString(s.str());
		}

		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_Uniquenick_Password;
		request.callback = m_login_auth_cb;
		request.peer = this;
		IncRef();
		request.profile.uniquenick = nick;
		request.profile.namespaceid = FESL_ACCOUNT_NAMESPACEID;
		request.user.partnercode = OS_EA_PARTNER_CODE;
		request.user.password = password;

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
		return true;
	}
	void Peer::handle_auth_callback_error(TaskShared::WebErrorDetails error_details, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		FESL_ERROR error = FESL_ERROR_AUTH_FAILURE;
		switch (error_details.response_code) {
		case TaskShared::WebErrorCode_UniqueNickInUse:
			error = FESL_ERROR_ACCOUNT_EXISTS;
			break;
		case TaskShared::WebErrorCode_NoSuchUser:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		case TaskShared::WebErrorCode_NickInvalid:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		default:
			break;
		}
		SendError(cmd_type, error, TXN);
	}
	void Peer::handle_profile_search_callback_error(TaskShared::WebErrorDetails error_details, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		/*
		EProfileResponseType_Success,
		EProfileResponseType_GenericError,
		EProfileResponseType_BadNick,
		EProfileResponseType_Bad_OldNick,
		EProfileResponseType_UniqueNick_Invalid,
		EProfileResponseType_UniqueNick_InUse,
		*/
		FESL_ERROR error = FESL_ERROR_SYSTEM_ERROR;
		switch (error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInvalid:
			case TaskShared::WebErrorCode_NickInvalid:
				SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name was invalid. Please change and try again.");
				return;
			case TaskShared::WebErrorCode_NickInUse:
				//SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name is in use. Please choose another name.");
				error = FESL_ERROR_ACCOUNT_EXISTS;
				break;
			default:
				error = FESL_ERROR_SYSTEM_ERROR;
			break;
		}
		SendError(cmd_type, error, TXN);
		
	}
	void Peer::m_login_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=Login\n";
			s << "lkey=" << auth_data.session_key << "\n";
			((Peer *)peer)->m_session_key = auth_data.session_key;
			s << "displayName=" << profile.uniquenick << "\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			if (((Peer *)peer)->m_encrypted_login_info.length()) {
				s << "encryptedLoginInfo=" << OS::url_encode(((Peer *)peer)->m_encrypted_login_info) << "\n";
			}
			((Peer *)peer)->m_logged_in = true;
			((Peer *)peer)->m_user = user;
			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

			TaskShared::ProfileRequest request;
			request.type = TaskShared::EProfileSearch_Profiles;
			request.user_search_details.id = user.id;
			request.user_search_details.partnercode = OS_EA_PARTNER_CODE;
			request.profile_search_details.namespaceid = FESL_PROFILE_NAMESPACEID;
			request.extra = peer;
			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((FESL::Server *)(peer->GetDriver()->getServer()))->GetProfileTask();
			scheduler->AddRequest(request.type, request);
		}
		else {
			((Peer *)peer)->handle_auth_callback_error(auth_data.error_details, FESL_TYPE_ACCOUNT, "Login");
		}
	}
	bool Peer::m_acct_login_sub_account(OS::KVReader kv_list) {
		loginToSubAccount(kv_list.GetValue("name"));
		return true;
	}
	bool Peer::m_acct_gamespy_preauth(OS::KVReader kv_list) {
		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_MakeAuthTicket;
		request.callback = m_create_auth_ticket;
		request.peer = this;
		IncRef();
		request.profile = m_profile;

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
		return true;
	}
	bool Peer::m_acct_send_account_name(OS::KVReader kv_list) {
		return true;
	}
	bool Peer::m_acct_send_account_password(OS::KVReader kv_list) {
		return true;
	}
}