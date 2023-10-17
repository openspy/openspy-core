#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::send_personas() {
		std::ostringstream s;
		uv_mutex_lock(&m_mutex);
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		s << "TXN=NuGetPersonas\n";
		if(m_last_profile_lookup_tid != -1)
			s << "TID=" << m_last_profile_lookup_tid << "\n";
		s << "personas.[]=" << m_profiles.size() << "\n";
		int i = 0;
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			s << "personas." << i++ << "=\"" << profile.uniquenick << "\"\n";
			it++;
		}
		uv_mutex_unlock(&m_mutex);
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
	}
	bool Peer::m_acct_get_personas(OS::KVReader kv_list) {
		int tid = -1;
		if(kv_list.HasKey("TID")) {
			tid = kv_list.GetValueInt("TID");
		}
		m_last_profile_lookup_tid = tid;
		if (!m_got_profiles) {
			m_pending_nuget_personas= true;
		}
		else {
			
			send_personas();
		}
		return true;
	}
}