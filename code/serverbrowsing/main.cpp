#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <uv.h>
#include <OS/Config/AppConfig.h>
#include <OS/Net/NetServer.h>
#include "server/SBServer.h"
#include "server/SBDriver.h"
INetServer *g_gameserver = NULL;
bool g_running = true;
void shutdown();

void on_exit(void) {
    shutdown();
}

#ifndef _WIN32
void sig_handler(int signo)
{
    if(signo == SIGTERM || signo == SIGINT) {
        shutdown();    
    }    
}
#endif

void idle_handler(uv_idle_t* handle) {
	g_gameserver->tick();
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_idle_t idler;

	uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, idle_handler);

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
	AppConfig *app_config = new AppConfig(cfg, "serverbrowsing");
	OS::Init("serverbrowsing", app_config);
	g_gameserver = new SBServer();

	std::vector<std::string> drivers = app_config->getDriverNames();
	std::vector<std::string>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		std::string s = *it;
		int version = 0;
		app_config->GetVariableInt(s, "protocol-version", version);

		
		bool proxyFlag = false;
		std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s, proxyFlag);
		OS::Address address = addresses.front();
		SB::Driver *driver = new SB::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort(), version, proxyFlag);
		OS::LogText(OS::ELogLevel_Info, "Adding SB Driver: %s Version: (%d) proxy: %d\n", address.ToString().c_str(), version, proxyFlag);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}

	g_gameserver->init();

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    printf("Shutting down!\n");

    delete g_gameserver;

    OS::Shutdown();
    return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}