#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Net/IOIfaces/SSLIOInterface.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
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

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *GetAuthTask() { return mp_auth_tasks; };
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *GetUserTask() { return mp_user_tasks; };
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *GetProfileTask() { return mp_profile_tasks; };
		TaskScheduler<TaskShared::GeoRequest, TaskThreadData> *GetGeoTasks() { return mp_geo_tasks; };

		TaskScheduler<FESLRequest, TaskThreadData> *GetFESLTasks() { return mp_fesl_tasks; };
		void GetCountries(std::vector<TaskShared::CountryRegion>::const_iterator &begin, std::vector<TaskShared::CountryRegion>::const_iterator &end) {
			begin = m_countries.begin();
			end = m_countries.end();
		}
	private:
		std::vector<TaskShared::CountryRegion> m_countries;
		static void GetCountriesCallback(TaskShared::GeoTaskData auth_data, void *extra, INetPeer *peer);
		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *mp_auth_tasks;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *mp_user_tasks;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *mp_profile_tasks;
		TaskScheduler<TaskShared::GeoRequest, TaskThreadData> *mp_geo_tasks;
		TaskScheduler<FESLRequest, TaskThreadData> *mp_fesl_tasks;
	};
}
#endif //_SMSERVER_H