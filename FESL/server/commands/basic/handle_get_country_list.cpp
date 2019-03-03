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
		FESL::Server *server = (FESL::Server *) GetDriver()->getServer();
		std::vector<TaskShared::CountryRegion>::const_iterator begin, end, it;
		server->GetCountries(begin, end);
		it = begin;
		int i = 0;
		while (it != end) {
			TaskShared::CountryRegion country = *it;
			s << "countryList." << i << ".description=\"" << country.countryname << "\"\n";
			s << "countryList." << i << ".ISOCode=\"" << country.countryname << "\"\n";
			i++;
			it++;
		}		

		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}