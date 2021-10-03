#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_nulogin_handler(OS::KVReader kv_list) {
		std::string nuid, password;
		if (kv_list.HasKey("encryptedInfo")) {
			m_encrypted_login_info = OS::url_decode(kv_list.GetValue("encryptedInfo"));
			kv_list = ((FESL::Driver *)this->GetDriver())->getStringCrypter()->decryptString(m_encrypted_login_info);
		}

		nuid = kv_list.GetValue("nuid");
		password = kv_list.GetValue("password");

		if (kv_list.GetValueInt("returnEncryptedInfo")) {
			std::ostringstream s;
			s << "\\nuid\\" << nuid;
			s << "\\password\\" << password;
			m_encrypted_login_info = ((FESL::Driver *)this->GetDriver())->getStringCrypter()->encryptString(s.str());
		}

		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_User_EmailPassword;
		request.callback = m_nulogin_auth_cb;
		request.peer = this;
		request.extra = (void *)-1;
		if(kv_list.HasKey("TID")) {
			request.extra = (void *)(ptrdiff_t)kv_list.GetValueInt("TID");
		}
		
		IncRef();

		request.profile.namespaceid = FESL_ACCOUNT_NAMESPACEID;
		request.user.partnercode = OS_EA_PARTNER_CODE;
		request.user.password = kv_list.GetValue("password");
		request.user.email = kv_list.GetValue("nuid");

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
		return true;
	}

	void Peer::m_nulogin_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=NuLogin\n";
			if((ptrdiff_t)extra != (ptrdiff_t)-1) {
				s << "TID=" << (ptrdiff_t)extra << "\n";
			}
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
			request.extra = extra;
			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((FESL::Server *)(peer->GetDriver()->getServer()))->GetProfileTask();
			scheduler->AddRequest(request.type, request);
		}
		else {
			((Peer *)peer)->handle_web_error(auth_data.error_details, FESL_TYPE_ACCOUNT, "NuLogin", (int)(ptrdiff_t)extra);
		}
	}
}