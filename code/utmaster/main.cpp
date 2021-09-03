#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include "server/UTServer.h"
#include "server/UTDriver.h"
INetServer *g_gameserver = NULL;
bool g_running = true;

void shutdown();

void on_exit(void) {
    shutdown();
}

void sig_handler(int signo)
{
    shutdown();
}

std::string get_file_contents(std::string path) {
	std::string ret;
	FILE *fd = fopen(path.c_str(),"r");
	if(fd) {
		fseek(fd,0,SEEK_END);
		int len = ftell(fd);
		fseek(fd,0,SEEK_SET);

		char *str_data = (char *)malloc(len+1);
		fread(str_data, len, 1, fd);
		str_data[len] = 0;
		ret = str_data;
		free((void *)str_data);
	}
	fclose(fd);
	return ret;
}

std::vector<UT::Config *> LoadConfigMapping(std::string filePath) {
	std::vector<UT::Config *> result;
	OS::Config *cfg = new OS::Config(filePath);

	OS::ConfigNode node;
	cfg->GetRootNode().FindObjectField("clientMappings", node);

	std::vector<OS::ConfigNode> children = node.GetArrayChildren();
	std::vector<OS::ConfigNode>::iterator it = children.begin();
	while(it != children.end()) {
		OS::ConfigNode child = *it;
		if (child.GetKey().compare("clientMapping") == 0) {
			UT::Config *msConfig = new UT::Config();

			std::vector<OS::ConfigNode> mappingChildren = child.GetArrayChildren();
			std::vector<OS::ConfigNode>::iterator it2 = mappingChildren.begin();
			while(it2 != mappingChildren.end()) {
				OS::ConfigNode configNode = *it2;
				std::string key = configNode.GetKey();
				if(key.compare("clientname") == 0) {
					msConfig->clientName = configNode.GetValue();
				} else if(key.compare("gameid") == 0) {
					msConfig->gameid = configNode.GetValueInt();
				} else if(key.compare("server") == 0) {
					msConfig->is_server = configNode.GetValueInt() == 1;
				} else if(key.compare("motd") == 0) {
					msConfig->motd = get_file_contents(configNode.GetValue());
				} else if(key.compare("latest-version") == 0) {
					msConfig->latest_version = atoi(configNode.GetValue().c_str());
				}
				it2++;
			}

			result.push_back(msConfig);
		}
		it++;
	}
	return result;
}

int main() {
	#ifndef _WIN32
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#else
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);
	#endif

		OS::Config *cfg = new OS::Config("openspy.xml");
		AppConfig *app_config = new AppConfig(cfg, "utmaster");
		OS::Init("utmaster", app_config);

		g_gameserver = new UT::Server();


		std::vector<std::string> drivers = app_config->getDriverNames();
		std::vector<std::string>::iterator it = drivers.begin();
		while (it != drivers.end()) {
			std::string s = *it;
			bool proxyFlag = false;
			std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s, proxyFlag);
			OS::Address address = addresses.front();
			std::string mappingLocation;
			app_config->GetVariableString(s, "client-mapping", mappingLocation);
			std::vector<UT::Config *> configMapping = LoadConfigMapping(mappingLocation);
			UT::Driver *driver = new UT::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort(), proxyFlag);
			driver->SetConfig(configMapping);
			OS::LogText(OS::ELogLevel_Info, "Adding utmaster Driver: %s:%d proxy: %d\n", address.ToString(true).c_str(), address.GetPort(), proxyFlag);
			g_gameserver->addNetworkDriver(driver);
			it++;
	}

	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}

    delete g_gameserver;

    OS::Shutdown();
    return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}