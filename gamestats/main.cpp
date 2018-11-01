#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include "server/GSServer.h"
#include "server/GSDriver.h"
#include "server/GSBackend.h"
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
		AppConfig *app_config = new AppConfig(cfg, "GS");
		OS::Init("GS", app_config);

		g_gameserver = new GS::Server();


		std::vector<std::string> drivers = app_config->getDriverNames();
		std::vector<std::string>::iterator it = drivers.begin();
		while (it != drivers.end()) {
			std::string s = *it;
			std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s);
			OS::Address address = addresses.front();
			GS::Driver *driver = new GS::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort());
			OS::LogText(OS::ELogLevel_Info, "Adding GS Driver: %s:%d\n", address.ToString(true).c_str(), address.GetPort());
			g_gameserver->addNetworkDriver(driver);
			it++;
	}
	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}
    delete g_gameserver;

	GSBackend::ShutdownTaskPool();
    OS::Shutdown();
    
    return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}