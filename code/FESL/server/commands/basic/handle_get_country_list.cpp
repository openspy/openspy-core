#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	const char* approved_countries[] = {
		"AU", "NZ", "PR", "ZA", "CN", "JP", "HK", "GU", "KR", "PH", "EG", "LY", "IN", "PK", "AF", "MY", "ID", "SG", "TH"
	};
	bool is_approved_country(std::string countryCode) {
		int num_approved = sizeof(approved_countries) / sizeof(const char *);
		for (int i = 0; i < num_approved; i++) {
			if(countryCode.compare(approved_countries[i]) == 0)
				return true;
		}
		return false;
	}
	bool Peer::m_acct_get_country_list(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetCountryList\n";
		if(kv_list.HasKey("TID")) {
			s << "TID=" << kv_list.GetValueInt("TID") << "\n";
		}
		FESL::Server *server = (FESL::Server *) GetDriver()->getServer();
		std::vector<TaskShared::CountryRegion>::const_iterator begin, end, it;
		server->GetCountries(begin, end);
		it = begin;
		int i = 0;
		int num_countries = std::distance(begin, end);
		while (it != end) {
			
			TaskShared::CountryRegion country = *it;
			bool is_approved = is_approved_country(country.countrycode);
			if(is_approved || country.region & REGIONID_AMERICAS || country.region & REGIONID_EUROPE) {
				if(is_approved || !(country.region & REGIONID_CARIBBEAN)) {
					s << "countryList." << i << ".description=\"" << country.countryname<< "\"\n";
					s << "countryList." << i << ".ISOCode=" << country.countrycode << "\n";				
					i++;
				}
			}
			it++;
		}		

		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}