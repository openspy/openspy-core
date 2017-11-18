#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/NNServer.h"
#include "server/NNPeer.h"
#include "server/NNDriver.h"
#include "server/NNBackend.h"
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

    OS::Init("natneg",0, "chc");

	#ifndef _WIN32
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
#else
    WSADATA wsdata;
    WSAStartup(MAKEWORD(1, 0), &wsdata);
#endif


	g_gameserver = new NN::Server();
    g_driver = new NN::Driver(g_gameserver, "0.0.0.0", NATNEG_PORT);
	g_gameserver->addNetworkDriver(g_driver);
	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}
    
    delete g_gameserver;
    delete g_driver;
    OS::Shutdown();
    NN::NNQueryTask::Shutdown();
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
    time =  ((uint64_t)file_time.dwLowDateTime );
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}


#endif

