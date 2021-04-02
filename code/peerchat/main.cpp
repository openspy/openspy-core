#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include <OS/SharedTasks/tasks.h>
#include "server/Server.h"
#include "server/Driver.h"
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
    int i = atexit(on_exit);
    if (i != 0) {
       fprintf(stderr, "cannot set exit function\n");
       exit(EXIT_FAILURE);
    }

	#ifndef _WIN32
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#else
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);
	#endif

		OS::Config *cfg = new OS::Config("openspy.xml");
		AppConfig *app_config = new AppConfig(cfg, "peerchat");
		OS::Init("peerchat", app_config);

		g_gameserver = new Peerchat::Server();


		std::vector<std::string> drivers = app_config->getDriverNames();
		std::vector<std::string>::iterator it = drivers.begin();
		while (it != drivers.end()) {
			std::string s = *it;
			bool proxyFlag = false;
			std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s, proxyFlag);
			OS::Address address = addresses.front();
			Peerchat::Driver *driver = new Peerchat::Driver(g_gameserver, s, address.ToString(true).c_str(), address.GetPort(), proxyFlag);
			OS::LogText(OS::ELogLevel_Info, "Adding peerchat (%s) Driver: %s:%d proxy: %d\n", s.c_str(), address.ToString(true).c_str(), address.GetPort(), proxyFlag);
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
