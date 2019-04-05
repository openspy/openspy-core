#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include "server/SMServer.h"
#include "server/SMDriver.h"
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

int main() {
	#ifndef _WIN32
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#else
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);
	#endif

		OS::Config *cfg = new OS::Config("openspy.xml");
		AppConfig *app_config = new AppConfig(cfg, "SM");
		OS::Init("SM", app_config);

		g_gameserver = new SM::Server();


		std::vector<std::string> drivers = app_config->getDriverNames();
		std::vector<std::string>::iterator it = drivers.begin();
		while (it != drivers.end()) {
			std::string s = *it;
			bool proxyHeaders = false;
			std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s, proxyHeaders);
			OS::Address address = addresses.front();
			SM::Driver *driver = new SM::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort(), proxyHeaders);
			OS::LogText(OS::ELogLevel_Info, "Adding SM Driver: %s:%d\n", address.ToString(true).c_str(), address.GetPort());
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