#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
typedef struct {
	int tid;
	int profileid;
} DisableSubAccountData;
namespace FESL {
	bool Peer::m_acct_disable_sub_account(OS::KVReader kv_list) {
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(kv_list.GetValue("name")) == 0) {
				TaskShared::ProfileRequest request;
				request.profile_search_details.id = profile.id;
				request.peer = this;
				

				DisableSubAccountData *data = (DisableSubAccountData *)malloc(sizeof(DisableSubAccountData));
				data->tid = tid;
				data->profileid = profile.id;
				request.extra = data;
				request.peer->IncRef();
				request.type = TaskShared::EProfileSearch_DeleteProfile;
				request.callback = Peer::m_delete_profile_callback;
				mp_mutex->unlock();
				TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetProfileTask();
				scheduler->AddRequest(request.type, request);
				return true;
			}
			it++;
		}
		SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_ACCOUNT_NOT_FOUND, "DisableSubAccount", tid);
		mp_mutex->unlock();
		return true;
	}
	void Peer::m_delete_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		DisableSubAccountData *data = (DisableSubAccountData *)extra;
		if (error_details.response_code == TaskShared::WebErrorCode_Success) {
			((Peer *)peer)->mp_mutex->lock();
			std::vector<OS::Profile>::iterator it = ((Peer *)peer)->m_profiles.begin();
			while (it != ((Peer *)peer)->m_profiles.end()) {
				OS::Profile profile = *it;
				if (profile.id == data->profileid) {
					((Peer *)peer)->m_profiles.erase(it);
					break;
				}
				it++;
			}
			((Peer *)peer)->mp_mutex->unlock();

			std::ostringstream s;
			s << "TXN=DisableSubAccount\n";

			if(data->tid != -1)
				s << "TID=" << data->tid << "\n";
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, (FESL_ERROR)FESL_ERROR_SYSTEM_ERROR, "DisableSubAccount", data->tid);
		}

		free((void *)data);
	}
}