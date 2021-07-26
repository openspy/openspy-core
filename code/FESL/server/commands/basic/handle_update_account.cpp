#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>

namespace FESL {
	class UpdateAccountInfo {
		public:
			OS::Profile profile;
			OS::User user;
			bool user_update;
			bool profile_update;
			int tid;

			TaskShared::WebErrorDetails user_response;
			TaskShared::WebErrorDetails profile_response;
	};
    bool Peer::m_acct_update_account(OS::KVReader kv_list) {
		UpdateAccountInfo *update_info = new UpdateAccountInfo;
		update_info->profile = m_account_profile;
		update_info->user = m_user;
		update_info->user_update = false;
		update_info->profile_update = false;
		/*
		Got EAMsg(103):
		TXN=UpdateAccount
		email=chc@thmods.com
		parentalEmail=
		countryCode=1
		eaMailFlag=1
		thirdPartyMailFlag=0
		*/

		update_info->tid = -1;
		if(kv_list.HasKey("TID")) {
			update_info->tid = kv_list.GetValueInt("TID");
		}

		if (kv_list.GetValue("email").compare(update_info->user.email) != 0) {
			update_info->user.email = kv_list.GetValue("email");
			update_info->user_update = true;
		}

		if (kv_list.GetValue("countryCode").compare(update_info->profile.countrycode) != 0) {
			update_info->profile.countrycode = kv_list.GetValue("countryCode");
			update_info->profile_update = true;
		}
		/*

		*/

		if (update_info->user_update) {
			TaskShared::UserRequest user_request;
			user_request.search_params = update_info->user;
			user_request.type = TaskShared::EUserRequestType_Update;
			user_request.extra = update_info;
			user_request.peer = this;
			user_request.peer->IncRef();
			user_request.callback = Peer::m_update_user_callback;
			TaskScheduler<TaskShared::UserRequest, TaskThreadData> *user_scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetUserTask();
			user_scheduler->AddRequest(user_request.type, user_request);
		}
		else if (update_info->profile_update) {
			TaskShared::ProfileRequest profile_request;
			profile_request.profile_search_details = update_info->profile;
			profile_request.extra = update_info;
			profile_request.peer = this;
			profile_request.peer->IncRef();
			profile_request.type = TaskShared::EProfileSearch_UpdateProfile;
			profile_request.callback = Peer::m_update_user_profile_callback;
			TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *profile_scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetProfileTask();
			profile_scheduler->AddRequest(profile_request.type, profile_request);
		} else {
			std::ostringstream s;
			s << "TXN=UpdateAccount\n";
			if(kv_list.HasKey("TID")) {
				s << "TID=" << kv_list.GetValueInt("TID") << "\n";
			}
			SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		return true;
	}
	void Peer::m_update_user_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		UpdateAccountInfo *update_info = (UpdateAccountInfo *)extra;
		update_info->profile_response = error_details;
		std::ostringstream s;

		if (update_info->profile_response.response_code == TaskShared::WebErrorCode_Success && (!update_info->user_update || update_info->user_response.response_code == TaskShared::WebErrorCode_Success)) {
			((FESL::Peer *)peer)->m_user = update_info->user;
			((FESL::Peer *)peer)->m_account_profile = update_info->profile;
			s << "TXN=UpdateAccount\n";
			if(update_info->tid != -1) {
				s << "TID="<< update_info->tid << "\n";
			}
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		else if(update_info->user_response.response_code != TaskShared::WebErrorCode_Success) {
			((Peer *)peer)->handle_web_error(update_info->user_response, FESL_TYPE_ACCOUNT, "UpdateAccount", update_info->tid);
		}
		else if (update_info->profile_response.response_code != TaskShared::WebErrorCode_Success) {
			((Peer *)peer)->handle_web_error(update_info->profile_response, FESL_TYPE_ACCOUNT, "UpdateAccount", update_info->tid);
		}


		delete update_info;
	}
	void Peer::m_update_user_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		UpdateAccountInfo *update_info = (UpdateAccountInfo *)extra;
		update_info->user_response = error_details;
		if (error_details.response_code == TaskShared::WebErrorCode_Success) {
			if (update_info->profile_update) {
				TaskShared::ProfileRequest profile_request;
				profile_request.profile_search_details = update_info->profile;
				profile_request.extra = update_info;
				profile_request.peer = peer;
				profile_request.peer->IncRef();
				profile_request.type = TaskShared::EProfileSearch_UpdateProfile;
				profile_request.callback = Peer::m_update_user_profile_callback;
				TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *profile_scheduler = ((FESL::Server *)(peer->GetDriver()->getServer()))->GetProfileTask();
				profile_scheduler->AddRequest(profile_request.type, profile_request);
				return;
			}
			else {
				std::ostringstream s;
				s << "TXN=UpdateAccount\n";
				if(update_info->tid != -1) {
					s << "TID="<< update_info->tid << "\n";
				}
				((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
			}
		}
		else {
			((Peer *)peer)->handle_web_error(error_details, FESL_TYPE_ACCOUNT, "UpdateAccount", update_info->tid);
		}
		delete update_info;
	}
	

}