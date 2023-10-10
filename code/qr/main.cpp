#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <uv.h>
#include <OS/Config/AppConfig.h>
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


	OS::Init("qr", NULL);

	g_gameserver = new QR::Server();

	const char *address = "0.0.0.0";
	uint16_t port = 27900;

	QR::Driver *driver = new QR::Driver(g_gameserver, address, port);

	OS::LogText(OS::ELogLevel_Info, "Adding QR Driver: %s:%d\n", address, port);
	g_gameserver->addNetworkDriver(driver);

  	g_gameserver->init();

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    delete g_gameserver;
    OS::Shutdown();
	return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}