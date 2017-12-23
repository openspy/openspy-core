#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/config.h>
#include <OS/Net/NetServer.h>
#include "server/QRServer.h"
#include "server/QRDriver.h"
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

	OS::Init("qr", "openspy.cfg");

	g_gameserver = new QR::Server();

	configVar *qr_struct = OS::g_config->getRootArray("qr");
	configVar *driver_struct = OS::g_config->getArrayArray(qr_struct, "drivers");
	std::list<configVar *> drivers = OS::g_config->getArrayVariables(driver_struct);
	std::list<configVar *>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		configVar *driver_arr = *it;
		const char *bind_ip = OS::g_config->getArrayString(driver_arr, "address");
		int bind_port = OS::g_config->getArrayInt(driver_arr, "port");
		QR::Driver *driver = new QR::Driver(g_gameserver, bind_ip, bind_port);

		OS::LogText(OS::ELogLevel_Info, "Adding QR Driver: %s:%d\n", bind_ip, bind_port);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}
  	g_gameserver->init();
  	while(g_running) {
  		g_gameserver->tick();
  	}

    delete g_gameserver;

    MM::Shutdown();
    OS::Shutdown();
}

void shutdown() {
    if(g_running) {
        g_gameserver->flagExit();
        g_running = false;
    }
}