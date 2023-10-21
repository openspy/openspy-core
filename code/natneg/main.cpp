#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/NNServer.h"
#include "server/NNDriver.h"
INetServer *g_gameserver = NULL;


void tick_handler(uv_timer_t* handle) {
	g_gameserver->tick();
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_timer_t tick_timer;

	uv_timer_init(uv_default_loop(), &tick_timer);
    uv_timer_start(&tick_timer, tick_handler, 0, 250);


	OS::Init("natneg");

	g_gameserver = new NN::Server();

	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_NATNEG_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);

		uint16_t port = 28900;
		if(uv_os_getenv("OPENSPY_NATNEG_BIND_PORT", (char *)&port_buff, &temp_env_sz) != UV_ENOENT) {
			port = atoi(port_buff);
		}

		NN::Driver *driver = new NN::Driver(g_gameserver, address_buff, port);

		OS::LogText(OS::ELogLevel_Info, "Adding natneg Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	} else {
		OS::LogText(OS::ELogLevel_Warning, "Missing natneg bind address environment variable");
	}

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    delete g_gameserver;
    OS::Shutdown();
	return 0;
}