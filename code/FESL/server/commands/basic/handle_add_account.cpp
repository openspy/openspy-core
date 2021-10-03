#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_add_account(OS::KVReader kv_list) {
		OS::User user;
		OS::Profile profile;
		
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}

		user.email = kv_list.GetValue("email");
		user.password = kv_list.GetValue("password");
		profile.uniquenick = kv_list.GetValue("name");
		profile.nick = profile.uniquenick;
		profile.countrycode = kv_list.GetValue("countryCode");
		profile.zipcode = kv_list.GetValueInt("zipCode");

		if(user.email.length() <= 0) {
			SendCustomError(FESL_TYPE_ACCOUNT, "AddAccount", "Account.EmailAddress", "The specified email was invalid. Please change it and try again.", tid);
			return true;
		}

		profile.birthday = OS::Date(kv_list.GetValueInt("DOBYear"), kv_list.GetValueInt("DOBMonth"), kv_list.GetValueInt("DOBDay"));

		profile.namespaceid = FESL_ACCOUNT_NAMESPACEID;
		user.partnercode = OS_EA_PARTNER_CODE;

		TaskShared::UserRequest req;
		req.type = TaskShared::EUserRequestType_Create;
		req.peer = this;

		req.extra = (void *)(ptrdiff_t)tid;
		req.peer->IncRef();

		req.registerCallback = m_newuser_cb;

		req.profile_params = profile;
		req.search_params = user;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(req.type, req);
		
		return true;
	}
	void Peer::m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer) {
		FESL_ERROR err_code = FESL_ERROR_NO_ERROR;
		int tid = (int)(ptrdiff_t)extra;
		if (auth_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			switch (auth_data.error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInUse:
				err_code = FESL_ERROR_ACCOUNT_EXISTS;
				break;
			case TaskShared::WebErrorCode_EmailInvalid:
				((Peer *)peer)->SendCustomError(FESL_TYPE_ACCOUNT, "AddAccount", "Account.EmailAddress", "The specified email was invalid. Please change it and try again.", tid);
				return;
			default:
				err_code = FESL_ERROR_SYSTEM_ERROR;
				break;
			}
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, (FESL_ERROR)err_code, "AddAccount", tid);
		}
		else {
			std::ostringstream s;
			s << "TXN=AddAccount\n";
			if(tid != -1) {
				s << "TID=" << tid << "\n";
			}
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

		}
	}
}