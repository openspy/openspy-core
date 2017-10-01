#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/socketlib/socketlib.h>
#include <OS/Net/NetServer.h>
#include "server/SBServer.h"
#include "server/SBDriver.h"
#include "server/MMQuery.h"
INetServer *g_gameserver = NULL;
SB::Driver *g_legacyms_driver, *g_ms_driver;
bool g_running = true;
void shutdown();

void on_exit(void) {
    shutdown();
}


void sig_handler(int signo)
{
    shutdown();
}


#include <OS/Analytics/AnalyticsMgr.h>
int main() {
    int i = atexit(on_exit);
    if (i != 0) {
       fprintf(stderr, "cannot set exit function\n");
       exit(EXIT_FAILURE);
    }
	#ifndef _WIN32
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#endif

	Socket::Init();
	OS::Init("serverbrowsing", 0, "chc");
    

	g_gameserver = new SBServer();
    g_legacyms_driver = new SB::Driver(g_gameserver, "0.0.0.0", 28900, 1);
    g_ms_driver = new SB::Driver(g_gameserver, "0.0.0.0", 28910, 2);

    g_gameserver->addNetworkDriver(g_legacyms_driver);
	g_gameserver->addNetworkDriver(g_ms_driver);

	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}

    printf("Shutting down!\n");

    delete g_gameserver;
    delete g_legacyms_driver;
    delete g_ms_driver;

    MM::MMQueryTask::Shutdown();
    OS::Shutdown();

    return 0;
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

