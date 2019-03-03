#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_get_account(OS::KVReader kv_list) {
		std::ostringstream s;

		FESL::Server *server = (FESL::Server *) GetDriver()->getServer();
		std::vector<TaskShared::CountryRegion>::const_iterator begin, end, it;
		server->GetCountries(begin, end);

		TaskShared::CountryRegion region;
		while (it != end) {
			TaskShared::CountryRegion country = *it;
			if (country.countrycode.compare(m_profile.countrycode) == 0) {
				country = region;
				break;
			}
			it++;
		}
		
		s << "TXN=GetAccount\n";
		s << "parentalEmail=parents@ea.com\n";
		s << "countryCode=" << m_profile.countrycode << "\n";
		s << "countryDesc=\"" << region.countryname << "\"\n";
		s << "thirdPartyMailFlag=0\n";
		s << "dobDay=" << (int)m_profile.birthday.GetDay() << "\n";
		s << "dobMonth=" << (int)m_profile.birthday.GetMonth() << "\n";
		s << "dobYear=" << (int)m_profile.birthday.GetYear() << "\n";
		s << "name=" << m_profile.nick << "\n";
		s << "email=" << m_user.email << "\n";
		s << "profileID=" << m_profile.id << "\n";
		s << "userId=" << m_user.id<< "\n";
		s << "zipCode=" << m_profile.zipcode << "\n";
		s << "gender=" << ((m_profile.sex == 0) ? 'M' : 'F') << "\n";
		s << "eaMailFlag=0\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}