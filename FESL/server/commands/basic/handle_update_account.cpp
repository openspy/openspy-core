#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>

namespace FESL {

    bool Peer::m_acct_update_account(OS::KVReader kv_list) {
		/*
		Got EAMsg(103):
		TXN=UpdateAccount
		email=chc@thmods.com
		parentalEmail=
		countryCode=1
		eaMailFlag=1
		thirdPartyMailFlag=0
		*/

		OS::Profile profile = m_profile;
		OS::User user = m_user;
		bool send_userupdate = false; //, send_profileupdate = false;

		if (kv_list.GetValue("email").compare(m_user.email) == 0) {
			user.email = kv_list.GetValue("email");
			send_userupdate = true;
		}
		/*
		OS::ProfileSearchRequest request;
		request.profile_search_details = m_profile;
		request.extra = NULL;
		request.peer = this;
		request.peer->IncRef();
		request.type = OS::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_profile_callback;
		OS::m_profile_search_task_pool->AddRequest(request);
		*/

		if (send_userupdate) {
			TaskShared::UserRequest user_request;
			user_request.search_params = user;
			user_request.type = TaskShared::EUserRequestType_Update;
			user_request.extra = NULL;
			user_request.peer = this;
			user_request.peer->IncRef();
			user_request.callback = Peer::m_update_user_callback;
			TaskScheduler<TaskShared::UserRequest, TaskThreadData> *user_scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetUserTask();
			user_scheduler->AddRequest(user_request.type, user_request);
		}
		return true;
	}
	void Peer::m_update_user_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		std::ostringstream s;
		s << "TXN=UpdateAccount\n";
		if (error_details.response_code == TaskShared::WebErrorCode_Success) {
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_SYSTEM_ERROR, "UpdateAccount");
		}
	}

}