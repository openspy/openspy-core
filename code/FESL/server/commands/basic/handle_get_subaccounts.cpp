#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::send_subaccounts() {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		int i = 0;
		s << "TXN=GetSubAccounts\n";
		s << "subAccounts.[]=" << m_profiles.size() << "\n";
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			s << "subAccounts." << i++ << "=\"" << profile.uniquenick << "\"\n";
			it++;
		}
		
		mp_mutex->unlock();
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
	}
	bool Peer::m_acct_get_sub_accounts(OS::KVReader kv_list) {
		if (!m_got_profiles) {
			m_pending_subaccounts = true;
		}
		else {
			send_subaccounts();
		}
		return true;
	}
}