#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <server/tasks/tasks.h>

namespace FESL {
	class FESLRequest;
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void init();
		void tick();
		void shutdown();
		void OnUserAuth(std::string session_key, int userid, int profileid);
		std::vector<TaskShared::CountryRegion> GetCountries() {
			return m_countries;
		}
	private:
		std::vector<TaskShared::CountryRegion> m_countries;
		static void GetCountriesCallback(TaskShared::GeoTaskData auth_data, void *extra, INetPeer *peer);
	};
}
#endif //_SMSERVER_H