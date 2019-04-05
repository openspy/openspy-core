#ifndef _APP_CONFIG_H
#define _APP_CONFIG_H
#include <OS/OpenSpy.h>
#include "Config.h"
class AppConfig {
	public:
		AppConfig(OS::Config *cfg, std::string appName);
		~AppConfig();

		std::vector<std::string> getDriverNames();

		bool GetVariableString(std::string driverName , std::string name, std::string &out);
		bool GetVariableInt(std::string driverName, std::string name, int &out);

		std::vector<OS::Address> GetDriverAddresses(std::string driverName, bool &proxyFlag);

	private:
		std::string GetVariableValue(OS::ConfigNode node);
		std::map<std::string, std::string> getCombinedVariables(std::string driverName);
		bool getDriverNode(std::string name, OS::ConfigNode &return_node);
		OS::Config *mp_config;
		std::string m_app_name;
};
#endif //_APP_CONFIG_H