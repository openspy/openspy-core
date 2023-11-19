#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include "server/GPServer.h"
#include "server/GPDriver.h"
#include "tasks/tasks.h"
INetServer *g_gameserver = NULL;

void tick_handler(uv_timer_t* handle) {
	g_gameserver->tick();
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_timer_t tick_timer;

	uv_timer_init(uv_default_loop(), &tick_timer);

	OS::Init("GP");

	g_gameserver = new GP::Server();


	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_GP_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);

		uint16_t port = 29900;
		if(uv_os_getenv("OPENSPY_GP_BIND_PORT", (char *)&port_buff, &temp_env_sz) != UV_ENOENT) {
			port = atoi(port_buff);
		}

		GP::Driver *driver = new GP::Driver(g_gameserver, address_buff, port);

		OS::LogText(OS::ELogLevel_Info, "Adding GP Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	} else {
		OS::LogText(OS::ELogLevel_Warning, "Missing GP bind address environment variable");
	}

	GP::InitTasks();
	uv_timer_start(&tick_timer, tick_handler, 0, 250);

	uv_run(loop, UV_RUN_DEFAULT);

	uv_loop_close(loop);
	
	delete g_gameserver;

	OS::Shutdown();
	return 0;
}