#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/SBServer.h"
#include "server/SBDriver.h"
#include "server/MMQuery.h"
INetServer *g_gameserver = NULL;
bool g_running = true;
void shutdown();

void on_exit(void) {
    shutdown();
}

void debug_dump() {
    ((SBServer *)g_gameserver)->debug_dump();
}
#ifndef _WIN32
void sig_handler(int signo)
{
    if(signo == SIGTERM) {
        shutdown();    
    } else if(signo == SIGINT) {
        debug_dump();
    }
    
}
#endif

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
	AppConfig *app_config = new AppConfig(cfg, "serverbrowsing");
	OS::Init("serverbrowsing", app_config);
	g_gameserver = new SBServer();

	std::vector<std::string> drivers = app_config->getDriverNames();
	std::vector<std::string>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		std::string s = *it;
		int version = 0;
		app_config->GetVariableInt(s, "protocol-version", version);
		const char* bind_ip = "0.0.0.0";
		uint16_t bind_port = htons(28910);
		SB::Driver *driver = new SB::Driver(g_gameserver, bind_ip, bind_port, version);
		OS::LogText(OS::ELogLevel_Info, "Adding SB Driver: %s:%d Version: (%d)\n", bind_ip, bind_port, version);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}
	/*
	
    
	g_gameserver = new SBServer();
	
	configVar *sb_struct = OS::g_config->getRootArray("serverbrowsing");
	configVar *driver_struct = OS::g_config->getArrayArray(sb_struct, "drivers");
	std::list<configVar *> drivers = OS::g_config->getArrayVariables(driver_struct);
	std::list<configVar *>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		configVar *driver_arr = *it;
		const char *bind_ip = OS::g_config->getArrayString(driver_arr, "address");
		int bind_port = OS::g_config->getArrayInt(driver_arr, "port");
		int version = OS::g_config->getArrayInt(driver_arr, "version");
		SB::Driver *driver = new SB::Driver(g_gameserver, bind_ip, bind_port, version);
		OS::LogText(OS::ELogLevel_Info, "Adding SB Driver: %s:%d Version: (%d)\n", bind_ip, bind_port, version);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}
	*/

	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}

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