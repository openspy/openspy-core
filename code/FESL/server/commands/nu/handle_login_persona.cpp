#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::loginToPersona(std::string uniquenick, int tid) {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(uniquenick) == 0) {
				m_profile = profile;
				s << "TXN=NuLoginPersona\n";
				if(tid != -1) {
					s << "TID=" << tid << "\n";
				}
				s << "lkey=" << m_session_key << "\n";
				s << "profileId=" << m_profile.id << "\n";
				s << "userId=" << m_user.id << "\n";
				SendPacket(FESL_TYPE_ACCOUNT, s.str());
				break;
			}
			it++;
		}
		mp_mutex->unlock();
	}
	bool Peer::m_acct_login_persona(OS::KVReader kv_list) {
		std::ostringstream s;
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}
		loginToPersona(kv_list.GetValue("name"), tid);
		return true;
	}
}