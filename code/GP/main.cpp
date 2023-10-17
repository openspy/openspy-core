#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/GPServer.h"
#include "server/GPDriver.h"
INetServer *g_gameserver = NULL;

void idle_handler(uv_idle_t* handle) {
	g_gameserver->tick();
}


int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_idle_t idler;

	uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, idle_handler);


	OS::Init("GP");

	g_gameserver = new GP::Server();


	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_GP_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);
		uv_os_getenv("OPENSPY_GP_BIND_PORT", (char *)&port_buff, &temp_env_sz);
		uint16_t port = atoi(port_buff);

		GP::Driver *driver = new GP::Driver(g_gameserver, address_buff, port);

		OS::LogText(OS::ELogLevel_Info, "Adding GP Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	}


	g_gameserver->init();

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
	
    delete g_gameserver;

    OS::Shutdown();
    return 0;
}