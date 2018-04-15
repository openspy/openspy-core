#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/GPServer.h"
#include "server/GPDriver.h"
#include "server/GPBackend.h"
INetServer *g_gameserver = NULL;
INetDriver *g_driver = NULL;
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

	OS::Init("GP", "openspy.cfg");

	g_gameserver = new GP::Server();
	configVar *gp_struct = OS::g_config->getRootArray("GP");
	configVar *driver_struct = OS::g_config->getArrayArray(gp_struct, "drivers");
	std::list<configVar *> drivers = OS::g_config->getArrayVariables(driver_struct);
	std::list<configVar *>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		configVar *driver_arr = *it;
		const char *bind_ip = OS::g_config->getArrayString(driver_arr, "address");
		int bind_port = OS::g_config->getArrayInt(driver_arr, "port");
		GP::Driver *driver = new GP::Driver(g_gameserver, bind_ip, bind_port);
		OS::LogText(OS::ELogLevel_Info, "Adding GP Driver: %s:%d\n", bind_ip, bind_port);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}

	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}

    delete g_gameserver;
    delete g_driver;

	GPBackend::ShutdownTaskPool();
    OS::Shutdown();	
    return 0;

}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}
