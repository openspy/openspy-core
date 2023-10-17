#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::loginToSubAccount(std::string uniquenick, int tid) {
		std::ostringstream s;
		uv_mutex_lock(&m_mutex);
		bool loggedIn = false;
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(uniquenick) == 0) {
				m_profile = profile;
				s << "TXN=LoginSubAccount\n";
				if(tid != -1) {
					s << "TID=" << tid << "\n";
				}
				s << "lkey=" << m_session_key << "\n";
				s << "profileId=" << m_profile.id << "\n";
				s << "userId=" << m_user.id << "\n";
				SendPacket(FESL_TYPE_ACCOUNT, s.str());
				loggedIn = true;
				break;
			}
			it++;
		}
		if (!loggedIn) {
			SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_ACCOUNT_NOT_FOUND, "LoginSubAccount", tid);
		}
		uv_mutex_unlock(&m_mutex);
	}
	bool Peer::m_acct_login_sub_account(OS::KVReader kv_list) {
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}
		loginToSubAccount(kv_list.GetValue("name"), tid);
		return true;
	}
}