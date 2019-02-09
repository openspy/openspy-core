#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_get_country_list(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetCountryList\n";
		s << "countryList.0.description=\"North America\"\n";
		s << "countryList.0.ISOCode=1\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}