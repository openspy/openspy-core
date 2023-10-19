#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>

#include <OS/Net/NetPeer.h>
#include "server/FESLServer.h"
#include "server/FESLDriver.h"
#include <SSL/StringCrypter.h>
INetServer *g_gameserver = NULL;


void tick_handler(uv_timer_t* handle) {
	g_gameserver->tick();
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_timer_t tick_timer;

	uv_timer_init(uv_default_loop(), &tick_timer);
    uv_timer_start(&tick_timer, tick_handler, 0, 250);

	OS::Init("FESL");
	g_gameserver = new FESL::Server();


	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_FESL_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);
		uv_os_getenv("OPENSPY_FESL_BIND_PORT", (char *)&port_buff, &temp_env_sz);
		uint16_t port = atoi(port_buff);

		FESL::PublicInfo public_info;
		std::string str_crypter_rsa_key;

		FESL::Driver *driver = new FESL::Driver(g_gameserver, address_buff, port, public_info, str_crypter_rsa_key);

		OS::LogText(OS::ELogLevel_Info, "Adding FESL Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	}



	// std::string stringCrypterPKey;
	// app_config->GetVariableString(s, "stringCrypterPKey", stringCrypterPKey);

	// FESL::PublicInfo server_info;

	// std::string tos_path;
	// app_config->GetVariableString(s, "tosFile", tos_path);
	// app_config->GetVariableString(s, "domainPartition", server_info.domainPartition);
	// app_config->GetVariableString(s, "subDomain", server_info.subDomain);

	// app_config->GetVariableString(s, "messagingHostname", server_info.messagingHostname);
	
	// int port;
	// app_config->GetVariableInt(s, "messagingPort", port);
	// server_info.messagingPort = (uint16_t)port;

	// app_config->GetVariableString(s, "theaterHostname", server_info.theaterHostname);
	// app_config->GetVariableInt(s, "theaterPort", port);
	// server_info.theaterPort = (uint16_t)port;

	// app_config->GetVariableInt(s, "gameid", port);
	// server_info.gameid = (uint16_t)port;

	// server_info.termsOfServiceData = get_file_contents(tos_path);

	// FESL::Driver *driver = new FESL::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort(), server_info, stringCrypterPKey, x509_path.c_str(), rsa_path.c_str(), ssl_version, proxyFlag);
	// OS::LogText(OS::ELogLevel_Info, "Adding FESL Driver: %s (ssl: %d) proxy flag: %d, gameid: %d\n", address.ToString().c_str(), ssl_version != OS::ESSL_None, proxyFlag, server_info.gameid);
	// g_gameserver->addNetworkDriver(driver);

  	g_gameserver->init();

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    delete g_gameserver;
    OS::Shutdown();
	return 0;
}