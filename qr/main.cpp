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


#ifdef _WIN32

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}


#endif
