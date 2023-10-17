#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include <OS/SharedTasks/tasks.h>
#include "server/Server.h"
#include "server/Driver.h"
INetServer *g_gameserver = NULL;

void idle_handler(uv_idle_t* handle) {
	g_gameserver->tick();
}


int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_idle_t idler;

	uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, idle_handler);


	OS::Init("Peerchat", NULL);

	g_gameserver = new Peerchat::Server();


	char address_buff[256];
	char port_buff[16];
	char server_name_buff[256];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_PEERCHAT_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);
		uv_os_getenv("OPENSPY_PEERCHAT_BIND_PORT", (char *)&port_buff, &temp_env_sz);
		uint16_t port = atoi(port_buff);

		temp_env_sz = sizeof(server_name_buff);
		uv_os_getenv("OPENSPY_PEERCHAT_SERVER_NAME", (char *)&server_name_buff, &temp_env_sz);

		Peerchat::Driver *driver = new Peerchat::Driver(g_gameserver, server_name_buff, address_buff, port, false);

		OS::LogText(OS::ELogLevel_Info, "Adding Peerchat Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	}


	g_gameserver->init();

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
	
    delete g_gameserver;

    OS::Shutdown();
    return 0;
}